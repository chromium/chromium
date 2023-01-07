// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TARGETS_AFFINITY_H_
#define COMPONENTS_ZUCCHINI_TARGETS_AFFINITY_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <vector>

#include "components/zucchini/image_utils.h"

namespace zucchini {

class EquivalenceMap;

// Computes and stores affinity between old and new targets for a single target
// pool. This is only used during patch generation.
class TargetsAffinity {
 public:
  TargetsAffinity();
  TargetsAffinity(const TargetsAffinity&) = delete;
  const TargetsAffinity& operator=(const TargetsAffinity&) = delete;
  ~TargetsAffinity();

  // Infers affinity between |old_targets| and |new_targets| using similarities
  // described by |equivalence_map|, and updates internal state for retrieval of
  // affinity scores. Both |old_targets| and |new_targets| are targets in the
  // same pool and are sorted in ascending order.
  void InferFromSimilarities(const EquivalenceMap& equivalence_map,
                             const std::deque<offset_t>& old_targets,
                             const std::deque<offset_t>& new_targets);

  // Assigns labels to targets based on associations previously inferred, using
  // |min_affinity| to reject associations with weak |affinity|. Label 0 is
  // assigned to unassociated targets. Labels for old targets are written to
  // |old_labels| and labels for new targets are written to |new_labels|.
  // Returns the upper bound on assigned labels (>= 1 since 0 is used).
  uint32_t AssignLabels(double min_affinity,
                        std::vector<uint32_t>* old_labels,
                        std::vector<uint32_t>* new_labels);

  // Returns the affinity score between targets identified by |old_key| and
  // |new_keys|. Affinity > 0 means an association is likely, < 0 means
  // incompatible association, and 0 means neither targets have been associated.
  double AffinityBetween(key_t old_key, key_t new_key) const;

 private:
  struct Association {
    key_t other = 0;
    double affinity = 0.0;
  };

  // Forward and backward associations between old and new targets. For each
  // Association element, if |affinity == 0.0| then no association is defined
  // (and |other| is meaningless|. Otherwise |affinity > 0.0|, and the
  // association between |old_labels[old_key]| and |new_labels[new_key]| is
  // represented by:
  //   forward_association_[old_key].other == new_key;
  //   backward_association_[new_key].other == old_key;
  //   forward_association_[old_key].affinity ==
  //       backward_association_[new_key].affinity;
  // The two lists contain the same information, but having both enables quick
  // lookup, given |old_key| or |new_key|.
  std::vector<Association> forward_association_;
  std::vector<Association> backward_association_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TARGETS_AFFINITY_H_
