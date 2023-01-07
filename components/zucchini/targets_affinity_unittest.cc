// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/targets_affinity.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "components/zucchini/equivalence_map.h"
#include "components/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(TargetsAffinityTest, AffinityBetween) {
  using AffinityVector = std::vector<std::vector<double>>;

  // A common TargetsAffinity is used across independent tests. This is to
  // reflect actual usage, in which common TargetsAffinity is used so that
  // internal buffers get reused.
  TargetsAffinity targets_affinity;

  auto test_affinity = [&targets_affinity](
                           const EquivalenceMap& equivalence_map,
                           const std::deque<offset_t>& old_targets,
                           const std::deque<offset_t>& new_targets) {
    targets_affinity.InferFromSimilarities(equivalence_map, old_targets,
                                           new_targets);
    AffinityVector affinities(old_targets.size());
    for (key_t i = 0; i < old_targets.size(); ++i) {
      for (key_t j = 0; j < new_targets.size(); ++j) {
        affinities[i].push_back(targets_affinity.AffinityBetween(i, j));
      }
    }
    return affinities;
  };

  EXPECT_EQ(AffinityVector({}), test_affinity(EquivalenceMap(), {}, {}));
  EXPECT_EQ(AffinityVector({}),
            test_affinity(EquivalenceMap({{{0, 0, 8}, 1.0}}), {}, {}));

  EXPECT_EQ(AffinityVector({{0.0, 0.0}, {0.0, 0.0}}),
            test_affinity(EquivalenceMap(), {0, 10}, {0, 5}));

  EXPECT_EQ(AffinityVector({{1.0, -1.0}, {-1.0, 0.0}}),
            test_affinity(EquivalenceMap({{{0, 0, 1}, 1.0}}), {0, 10}, {0, 5}));

  EXPECT_EQ(AffinityVector({{1.0, -1.0}, {-1.0, 0.0}}),
            test_affinity(EquivalenceMap({{{0, 0, 2}, 1.0}}), {1, 10}, {1, 5}));

  EXPECT_EQ(AffinityVector({{0.0, 0.0}, {0.0, 0.0}}),
            test_affinity(EquivalenceMap({{{0, 1, 2}, 1.0}}), {1, 10}, {1, 5}));

  EXPECT_EQ(AffinityVector({{1.0, -1.0}, {-1.0, 0.0}}),
            test_affinity(EquivalenceMap({{{0, 1, 2}, 1.0}}), {0, 10}, {1, 5}));

  EXPECT_EQ(AffinityVector({{2.0, -2.0}, {-2.0, 0.0}}),
            test_affinity(EquivalenceMap({{{0, 0, 1}, 2.0}}), {0, 10}, {0, 5}));

  EXPECT_EQ(
      AffinityVector({{1.0, -1.0}, {-1.0, 1.0}, {-1.0, -1.0}}),
      test_affinity(EquivalenceMap({{{0, 0, 6}, 1.0}}), {0, 5, 10}, {0, 5}));

  EXPECT_EQ(AffinityVector({{-2.0, 2.0}, {1.0, -2.0}, {-1.0, -2.0}}),
            test_affinity(EquivalenceMap({{{5, 0, 2}, 1.0}, {{0, 5, 2}, 2.0}}),
                          {0, 5, 10}, {0, 5}));

  EXPECT_EQ(AffinityVector({{-2.0, 2.0}, {0.0, -2.0}, {0.0, -2.0}}),
            test_affinity(EquivalenceMap({{{0, 0, 2}, 1.0}, {{0, 5, 2}, 2.0}}),
                          {0, 5, 10}, {0, 5}));
}

TEST(TargetsAffinityTest, AssignLabels) {
  // A common TargetsAffinity is used across independent tests. This is to
  // reflect actual usage, in which common TargetsAffinity is used so that
  // internal buffers get reused.
  TargetsAffinity targets_affinity;

  auto test_labels_assignment =
      [&targets_affinity](const EquivalenceMap& equivalence_map,
                          const std::deque<offset_t>& old_targets,
                          const std::deque<offset_t>& new_targets,
                          double min_affinity,
                          const std::vector<uint32_t>& expected_old_labels,
                          const std::vector<uint32_t>& expected_new_labels) {
        targets_affinity.InferFromSimilarities(equivalence_map, old_targets,
                                               new_targets);
        std::vector<uint32_t> old_labels;
        std::vector<uint32_t> new_labels;
        size_t bound = targets_affinity.AssignLabels(min_affinity, &old_labels,
                                                     &new_labels);
        EXPECT_EQ(expected_old_labels, old_labels);
        EXPECT_EQ(expected_new_labels, new_labels);
        return bound;
      };

  EXPECT_EQ(1U, test_labels_assignment(EquivalenceMap(), {}, {}, 1.0, {}, {}));
  EXPECT_EQ(1U, test_labels_assignment(EquivalenceMap({{{0, 0, 8}, 1.0}}), {},
                                       {}, 1.0, {}, {}));

  EXPECT_EQ(1U, test_labels_assignment(EquivalenceMap(), {0, 10}, {0, 5}, 1.0,
                                       {0, 0}, {0, 0}));

  EXPECT_EQ(2U, test_labels_assignment(EquivalenceMap({{{0, 0, 1}, 1.0}}),
                                       {0, 10}, {0, 5}, 1.0, {1, 0}, {1, 0}));
  EXPECT_EQ(1U, test_labels_assignment(EquivalenceMap({{{0, 0, 1}, 0.99}}),
                                       {0, 10}, {0, 5}, 1.0, {0, 0}, {0, 0}));
  EXPECT_EQ(1U, test_labels_assignment(EquivalenceMap({{{0, 0, 1}, 1.0}}),
                                       {0, 10}, {0, 5}, 1.01, {0, 0}, {0, 0}));
  EXPECT_EQ(1U, test_labels_assignment(EquivalenceMap({{{0, 0, 1}, 1.0}}),
                                       {0, 10}, {0, 5}, 15.0, {0, 0}, {0, 0}));
  EXPECT_EQ(2U, test_labels_assignment(EquivalenceMap({{{0, 0, 1}, 15.0}}),
                                       {0, 10}, {0, 5}, 15.0, {1, 0}, {1, 0}));

  EXPECT_EQ(2U, test_labels_assignment(EquivalenceMap({{{0, 1, 2}, 1.0}}),
                                       {0, 10}, {1, 5}, 1.0, {1, 0}, {1, 0}));
  EXPECT_EQ(
      3U, test_labels_assignment(EquivalenceMap({{{0, 0, 6}, 1.0}}), {0, 5, 10},
                                 {0, 5}, 1.0, {1, 2, 0}, {1, 2}));
  EXPECT_EQ(3U, test_labels_assignment(
                    EquivalenceMap({{{5, 0, 2}, 1.0}, {{0, 5, 2}, 2.0}}),
                    {0, 5, 10}, {0, 5}, 1.0, {1, 2, 0}, {2, 1}));
  EXPECT_EQ(2U, test_labels_assignment(
                    EquivalenceMap({{{0, 0, 2}, 1.0}, {{0, 5, 2}, 2.0}}),
                    {0, 5, 10}, {0, 5}, 1.0, {1, 0, 0}, {0, 1}));
}

}  // namespace zucchini
