// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/targets_affinity.h"

#include <algorithm>

#include "base/check_op.h"
#include "components/zucchini/equivalence_map.h"

namespace zucchini {

namespace {

constexpr uint32_t kNoLabel = 0;
}

TargetsAffinity::TargetsAffinity() = default;
TargetsAffinity::~TargetsAffinity() = default;

void TargetsAffinity::InferFromSimilarities(
    const EquivalenceMap& equivalences,
    const std::deque<offset_t>& old_targets,
    const std::deque<offset_t>& new_targets) {
  forward_association_.assign(old_targets.size(), {});
  backward_association_.assign(new_targets.size(), {});

  if (old_targets.empty() || new_targets.empty())
    return;

  key_t new_key = 0;
  for (auto candidate : equivalences) {  // Sorted by |dst_offset|.
    DCHECK_GT(candidate.similarity, 0.0);
    while (new_key < new_targets.size() &&
           new_targets[new_key] < candidate.eq.dst_offset) {
      ++new_key;
    }

    // Visit each new target covered by |candidate.eq| and find / update its
    // associated old target.
    for (; new_key < new_targets.size() &&
           new_targets[new_key] < candidate.eq.dst_end();
         ++new_key) {
      if (backward_association_[new_key].affinity >= candidate.similarity)
        continue;

      DCHECK_GE(new_targets[new_key], candidate.eq.dst_offset);
      offset_t old_target = new_targets[new_key] - candidate.eq.dst_offset +
                            candidate.eq.src_offset;
      auto old_it =
          std::lower_bound(old_targets.begin(), old_targets.end(), old_target);
      // If new target can be mapped via |candidate.eq| to an old target, then
      // attempt to associate them. Multiple new targets can compete for the
      // same old target. The heuristic here makes selections to maximize
      // |candidate.similarity|, and if a tie occurs, minimize new target offset
      // (by first-come, first-served).
      if (old_it != old_targets.end() && *old_it == old_target) {
        key_t old_key = static_cast<key_t>(old_it - old_targets.begin());
        if (candidate.similarity > forward_association_[old_key].affinity) {
          // Reset other associations.
          if (forward_association_[old_key].affinity > 0.0)
            backward_association_[forward_association_[old_key].other] = {};
          if (backward_association_[new_key].affinity > 0.0)
            forward_association_[backward_association_[new_key].other] = {};
          // Assign new association.
          forward_association_[old_key] = {new_key, candidate.similarity};
          backward_association_[new_key] = {old_key, candidate.similarity};
        }
      }
    }
  }
}

uint32_t TargetsAffinity::AssignLabels(double min_affinity,
                                       std::vector<uint32_t>* old_labels,
                                       std::vector<uint32_t>* new_labels) {
  old_labels->assign(forward_association_.size(), kNoLabel);
  new_labels->assign(backward_association_.size(), kNoLabel);

  uint32_t label = kNoLabel + 1;
  for (key_t old_key = 0; old_key < forward_association_.size(); ++old_key) {
    Association association = forward_association_[old_key];
    if (association.affinity >= min_affinity) {
      (*old_labels)[old_key] = label;
      DCHECK_EQ(0U, (*new_labels)[association.other]);
      (*new_labels)[association.other] = label;
      ++label;
    }
  }
  return label;
}

double TargetsAffinity::AffinityBetween(key_t old_key, key_t new_key) const {
  DCHECK_LT(old_key, forward_association_.size());
  DCHECK_LT(new_key, backward_association_.size());
  if (forward_association_[old_key].affinity > 0.0 &&
      forward_association_[old_key].other == new_key) {
    DCHECK_EQ(backward_association_[new_key].other, old_key);
    DCHECK_EQ(forward_association_[old_key].affinity,
              backward_association_[new_key].affinity);
    return forward_association_[old_key].affinity;
  }
  return -std::max(forward_association_[old_key].affinity,
                   backward_association_[new_key].affinity);
}

}  // namespace zucchini
