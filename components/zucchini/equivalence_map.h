// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_EQUIVALENCE_MAP_H_
#define COMPONENTS_ZUCCHINI_EQUIVALENCE_MAP_H_

#include <stddef.h>

#include <deque>
#include <limits>
#include <vector>

#include "components/zucchini/image_index.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/targets_affinity.h"

namespace zucchini {

constexpr double kMismatchFatal = -std::numeric_limits<double>::infinity();

class EncodedView;
class EquivalenceSource;

// Returns similarity score between a token (raw byte or first byte of a
// reference) in |old_image_index| at |src| and a token in |new_image_index|
// at |dst|. |targets_affinities| describes affinities for each target pool and
// is used to evaluate similarity between references, hence it's size must be
// equal to the number of pools in both |old_image_index| and |new_image_index|.
// Both |src| and |dst| must refer to tokens in |old_image_index| and
// |new_image_index|.
double GetTokenSimilarity(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const std::vector<TargetsAffinity>& targets_affinities,
    offset_t src,
    offset_t dst);

// Returns a similarity score between content in |old_image_index| and
// |new_image_index| at regions described by |equivalence|, using
// |targets_affinities| to evaluate similarity between references.
double GetEquivalenceSimilarity(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const std::vector<TargetsAffinity>& targets_affinities,
    const Equivalence& equivalence);

// Extends |equivalence| forward and returns the result. This is related to
// VisitEquivalenceSeed().
EquivalenceCandidate ExtendEquivalenceForward(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const std::vector<TargetsAffinity>& targets_affinities,
    const EquivalenceCandidate& equivalence,
    double min_similarity);

// Extends |equivalence| backward and returns the result. This is related to
// VisitEquivalenceSeed().
EquivalenceCandidate ExtendEquivalenceBackward(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const std::vector<TargetsAffinity>& targets_affinities,
    const EquivalenceCandidate& equivalence,
    double min_similarity);

// Creates an equivalence, starting with |src| and |dst| as offset hint, and
// extends it both forward and backward, trying to maximise similarity between
// |old_image_index| and |new_image_index|, and returns the result.
// |targets_affinities| is used to evaluate similarity between references.
// |min_similarity| describes the minimum acceptable similarity score and is
// used as threshold to discard bad equivalences.
EquivalenceCandidate VisitEquivalenceSeed(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const std::vector<TargetsAffinity>& targets_affinities,
    offset_t src,
    offset_t dst,
    double min_similarity);

// Container of pruned equivalences used to map offsets from |old_image| to
// offsets in |new_image|. Equivalences are pruned by cropping smaller
// equivalences to avoid overlaps, to make the equivalence map (for covered
// bytes in |old_image| and |new_image|) one-to-one.
class OffsetMapper {
 public:
  using const_iterator = std::deque<Equivalence>::const_iterator;

  // Constructors for various data sources. "Old" and "new" image sizes are
  // needed for bounds checks and to handle dangling targets.
  // - From a list of |equivalences|, already sorted (by |src_offset|) and
  //   pruned, useful for tests.
  OffsetMapper(std::deque<Equivalence>&& equivalences,
               offset_t old_image_size,
               offset_t new_image_size);
  // - From a generator, useful for Zucchini-apply.
  OffsetMapper(EquivalenceSource&& equivalence_source,
               offset_t old_image_size,
               offset_t new_image_size);
  // - From an EquivalenceMap that needs to be processed, useful for
  //   Zucchini-gen.
  OffsetMapper(const EquivalenceMap& equivalence_map,
               offset_t old_image_size,
               offset_t new_image_size);
  ~OffsetMapper();

  size_t size() const { return equivalences_.size(); }
  const_iterator begin() const { return equivalences_.begin(); }
  const_iterator end() const { return equivalences_.end(); }

  // Returns naive extended forward-projection of "old" |offset| that follows
  // |eq|'s delta. |eq| needs not cover |offset|.
  // - Averts underflow / overflow by clamping to |[0, new_image_size_)|.
  // - However, |offset| is *not* restricted to |[0, old_image_size_)|; the
  //   caller must to make the check (hence "naive").
  offset_t NaiveExtendedForwardProject(const Equivalence& unit,
                                       offset_t offset) const;

  // Returns an offset in |new_image| corresponding to |offset| in |old_image|.
  // Assumes |equivalences_| to be non-empty. Cases:
  // - If |offset| is covered (i.e., in an "old" block), then use the delta of
  //   the (unique) equivalence unit that covers |offset|.
  // - If |offset| is non-covered, but in |[0, old_image_size_)|, then find the
  //   nearest "old" block, use its delta, and avert underflow / overflow by
  //   clamping the result to |[0, new_image_size_)|.
  // - If |offset| is >= |new_image_size_| (a "fake offset"), then use
  //   |new_image_size_ - old_image_size_| as the delta.
  offset_t ExtendedForwardProject(offset_t offset) const;

  // Given sorted |offsets|, applies a projection in-place of all offsets that
  // are part of a pruned equivalence from |old_image| to |new_image|. Other
  // offsets are removed from |offsets|.
  void ForwardProjectAll(std::deque<offset_t>* offsets) const;

  // Accessor for testing.
  const std::deque<Equivalence> equivalences() const { return equivalences_; }

  // Sorts |equivalences| by |src_offset| and removes all source overlaps; so a
  // source location that was covered by some Equivalence would become covered
  // by exactly one Equivalence. Moreover, for the offset, the equivalence
  // corresponds to the largest (pre-pruning) covering Equivalence, and in case
  // of a tie, the Equivalence with minimal |src_offset|. |equivalences| may
  // change in size since empty Equivalences are removed.
  static void PruneEquivalencesAndSortBySource(
      std::deque<Equivalence>* equivalences);

 private:
  // |equivalences_| is pruned, i.e., no "old" blocks overlap (and no "new"
  // block overlaps). Also, it is sorted by "old" offsets.
  std::deque<Equivalence> equivalences_;
  const offset_t old_image_size_;
  const offset_t new_image_size_;
};

// Container of equivalences between |old_image_index| and |new_image_index|,
// sorted by |Equivalence::dst_offset|, only used during patch generation.
class EquivalenceMap {
 public:
  using const_iterator = std::vector<EquivalenceCandidate>::const_iterator;

  EquivalenceMap();
  // Initializes the object with |equivalences|.
  explicit EquivalenceMap(std::vector<EquivalenceCandidate>&& candidates);
  EquivalenceMap(EquivalenceMap&&);
  EquivalenceMap(const EquivalenceMap&) = delete;
  ~EquivalenceMap();

  // Finds relevant equivalences between |old_view| and |new_view|, using
  // suffix array |old_sa| computed from |old_view| and using
  // |targets_affinities| to evaluate similarity between references. This
  // function is not symmetric. Equivalences might overlap in |old_view|, but
  // not in |new_view|. It tries to maximize accumulated similarity within each
  // equivalence, while maximizing |new_view| coverage. The minimum similarity
  // of an equivalence is given by |min_similarity|.
  void Build(const std::vector<offset_t>& old_sa,
             const EncodedView& old_view,
             const EncodedView& new_view,
             const std::vector<TargetsAffinity>& targets_affinities,
             double min_similarity);

  size_t size() const { return candidates_.size(); }
  const_iterator begin() const { return candidates_.begin(); }
  const_iterator end() const { return candidates_.end(); }

 private:
  // Discovers equivalence candidates between |old_view| and |new_view| and
  // stores them in the object. Note that resulting candidates are not sorted
  // and might be overlapping in new image.
  void CreateCandidates(const std::vector<offset_t>& old_sa,
                        const EncodedView& old_view,
                        const EncodedView& new_view,
                        const std::vector<TargetsAffinity>& targets_affinities,
                        double min_similarity);
  // Sorts candidates by their offset in new image.
  void SortByDestination();
  // Visits |candidates_| (sorted by |dst_offset|) and remove all destination
  // overlaps. Candidates with low similarity scores are more likely to be
  // shrunken. Unfit candidates may be removed.
  void Prune(const EncodedView& old_view,
             const EncodedView& new_view,
             const std::vector<TargetsAffinity>& targets_affinities,
             double min_similarity);

  std::vector<EquivalenceCandidate> candidates_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_EQUIVALENCE_MAP_H_
