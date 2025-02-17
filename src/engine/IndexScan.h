// Copyright 2015, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)
#pragma once

#include <string>

#include "./Operation.h"

class SparqlTriple;
class SparqlTripleSimple;

class IndexScan final : public Operation {
 private:
  Permutation::Enum permutation_;
  TripleComponent subject_;
  TripleComponent predicate_;
  TripleComponent object_;
  using Graphs = ScanSpecificationAsTripleComponent::Graphs;
  Graphs graphsToFilter_;
  size_t numVariables_;
  size_t sizeEstimate_;
  vector<float> multiplicity_;

  // Additional columns (e.g. patterns) that are being retrieved in addition to
  // the "ordinary" subjects, predicates, or objects, as well as the variables
  // that they are bound to.
  std::vector<ColumnIndex> additionalColumns_;
  std::vector<Variable> additionalVariables_;

 public:
  IndexScan(QueryExecutionContext* qec, Permutation::Enum permutation,
            const SparqlTriple& triple, Graphs graphsToFilter = std::nullopt);
  IndexScan(QueryExecutionContext* qec, Permutation::Enum permutation,
            const SparqlTripleSimple& triple,
            Graphs graphsToFilter = std::nullopt);

  ~IndexScan() override = default;

  // Const getters for testing.
  const TripleComponent& predicate() const { return predicate_; }
  const TripleComponent& subject() const { return subject_; }
  const TripleComponent& object() const { return object_; }
  const auto& graphsToFilter() const { return graphsToFilter_; }
  const std::vector<Variable>& additionalVariables() const {
    return additionalVariables_;
  }

  const std::vector<ColumnIndex>& additionalColumns() const {
    return additionalColumns_;
  }
  std::string getDescriptor() const override;

  size_t getResultWidth() const override;

  vector<ColumnIndex> resultSortedOn() const override;

  size_t numVariables() const { return numVariables_; }

  // Return the exact result size of the index scan. This is always known as it
  // can be read from the Metadata.
  size_t getExactSize() const { return sizeEstimate_; }

  // Return two generators that lazily yield the results of `s1` and `s2` in
  // blocks, but only the blocks that can theoretically contain matching rows
  // when performing a join on the first column of the result of `s1` with the
  // first column of the result of `s2`.
  static std::array<Permutation::IdTableGenerator, 2> lazyScanForJoinOfTwoScans(
      const IndexScan& s1, const IndexScan& s2);

  // Return a generator that lazily yields the result in blocks, but only
  // the blocks that can theoretically contain matching rows when performing a
  // join between the first column of the result with the `joinColumn`.
  // Requires that the `joinColumn` is sorted, else the behavior is undefined.
  Permutation::IdTableGenerator lazyScanForJoinOfColumnWithScan(
      std::span<const Id> joinColumn) const;

 private:
  // TODO<joka921> Make the `getSizeEstimateBeforeLimit()` function `const` for
  // ALL the `Operations`.
  uint64_t getSizeEstimateBeforeLimit() override { return getExactSize(); }

 public:
  size_t getCostEstimate() override;

  void determineMultiplicities();

  float getMultiplicity(size_t col) override {
    if (multiplicity_.empty()) {
      determineMultiplicities();
    }
    AD_CORRECTNESS_CHECK(col < multiplicity_.size());
    return multiplicity_[col];
  }

  bool knownEmptyResult() override { return getExactSize() == 0; }

  bool isIndexScanWithNumVariables(size_t target) const override {
    return numVariables() == target;
  }

  // Full index scans will never be able to fit in the cache on datasets the
  // size of wikidata, so we don't even need to try and waste performance.
  bool unlikelyToFitInCache(
      ad_utility::MemorySize maxCacheableSize) const override {
    return ad_utility::MemorySize::bytes(getExactSize() * getResultWidth() *
                                         sizeof(Id)) > maxCacheableSize;
  }

  // An index scan can directly and efficiently support LIMIT and OFFSET
  [[nodiscard]] bool supportsLimit() const override { return true; }

  Permutation::Enum permutation() const { return permutation_; }

  // Return the stored triple in the order that corresponds to the
  // `permutation_`. For example if `permutation_ == PSO` then the result is
  // {&predicate_, &subject_, &object_}
  std::array<const TripleComponent* const, 3> getPermutedTriple() const;
  ScanSpecification getScanSpecification() const;
  ScanSpecificationAsTripleComponent getScanSpecificationTc() const;

 private:
  ProtoResult computeResult(bool requestLaziness) override;

  vector<QueryExecutionTree*> getChildren() override { return {}; }

  size_t computeSizeEstimate() const;

  std::string getCacheKeyImpl() const override;

  VariableToColumnMap computeVariableToColumnMap() const override;

  Result::Generator scanInChunks() const;

  //  Helper functions for the public `getLazyScanFor...` functions (see above).
  Permutation::IdTableGenerator getLazyScan(
      std::vector<CompressedBlockMetadata> blocks) const;
  std::optional<Permutation::MetadataAndBlocks> getMetadataForScan() const;
};
