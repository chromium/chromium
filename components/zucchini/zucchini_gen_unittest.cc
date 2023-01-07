// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/zucchini_gen.h"

#include <stdint.h>

#include <deque>
#include <utility>
#include <vector>

#include "components/zucchini/equivalence_map.h"
#include "components/zucchini/image_index.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

using OffsetVector = std::vector<offset_t>;

// In normal usage, 0.0 is an unrealistic similarity value for an
// EquivalenceCandiate. Since similarity doesn't affect results for various unit
// tests in this file, we use this dummy value for simplicity.
constexpr double kDummySim = 0.0;

// Helper function wrapping GenerateReferencesDelta().
std::vector<int32_t> GenerateReferencesDeltaTest(
    std::vector<Reference>&& old_references,
    std::vector<Reference>&& new_references,
    std::deque<offset_t>&& exp_old_targets,
    std::deque<offset_t>&& exp_projected_old_targets,
    EquivalenceMap&& equivalence_map) {
  // OffsetMapper needs image sizes for forward-projection overflow check. These
  // are tested elsewhere, so just use arbitrary large value.
  constexpr offset_t kOldImageSize = 1000000;
  constexpr offset_t kNewImageSize = 1001000;

  ReferenceDeltaSink reference_delta_sink;

  TargetPool old_targets;
  old_targets.InsertTargets(old_references);
  ReferenceSet old_refs({1, TypeTag(0), PoolTag(0)}, old_targets);
  old_refs.InitReferences(old_references);
  EXPECT_EQ(exp_old_targets, old_targets.targets());

  TargetPool new_targets;
  new_targets.InsertTargets(new_references);
  ReferenceSet new_refs({1, TypeTag(0), PoolTag(0)}, new_targets);
  new_refs.InitReferences(new_references);

  OffsetMapper offset_mapper(equivalence_map, kOldImageSize, kNewImageSize);
  TargetPool projected_old_targets = old_targets;
  projected_old_targets.FilterAndProject(offset_mapper);

  std::vector<offset_t> extra_target =
      FindExtraTargets(projected_old_targets, new_targets);
  projected_old_targets.InsertTargets(extra_target);
  EXPECT_EQ(exp_projected_old_targets, projected_old_targets.targets());

  GenerateReferencesDelta(old_refs, new_refs, projected_old_targets,
                          offset_mapper, equivalence_map,
                          &reference_delta_sink);

  // Serialize |reference_delta_sink| to patch format, and read it back as
  // std::vector<int32_t>.
  std::vector<uint8_t> buffer(reference_delta_sink.SerializedSize());
  BufferSink sink(buffer.data(), buffer.size());
  reference_delta_sink.SerializeInto(&sink);

  BufferSource source(buffer.data(), buffer.size());
  ReferenceDeltaSource reference_delta_source;
  EXPECT_TRUE(reference_delta_source.Initialize(&source));
  std::vector<int32_t> delta_vec;
  for (auto delta = reference_delta_source.GetNext(); delta.has_value();
       delta = reference_delta_source.GetNext()) {
    delta_vec.push_back(*delta);
  }
  EXPECT_TRUE(reference_delta_source.Done());
  return delta_vec;
}

}  // namespace

TEST(ZucchiniGenTest, FindExtraTargets) {
  EXPECT_EQ(OffsetVector(), FindExtraTargets({}, {}));
  EXPECT_EQ(OffsetVector(), FindExtraTargets(TargetPool({3}), {}));
  EXPECT_EQ(OffsetVector(), FindExtraTargets(TargetPool({3}), TargetPool({3})));
  EXPECT_EQ(OffsetVector({4}),
            FindExtraTargets(TargetPool({3}), TargetPool({4})));
  EXPECT_EQ(OffsetVector({4}),
            FindExtraTargets(TargetPool({3}), TargetPool({3, 4})));
  EXPECT_EQ(OffsetVector({4}),
            FindExtraTargets(TargetPool({2, 3}), TargetPool({3, 4})));
  EXPECT_EQ(OffsetVector({3, 5}),
            FindExtraTargets(TargetPool({2, 4}), TargetPool({3, 5})));
}

TEST(ZucchiniGenTest, GenerateReferencesDelta) {
  // No equivalences.
  EXPECT_EQ(std::vector<int32_t>(),
            GenerateReferencesDeltaTest({}, {}, {}, {}, EquivalenceMap()));
  EXPECT_EQ(std::vector<int32_t>(),
            GenerateReferencesDeltaTest({{10, 0}}, {{20, 0}}, {0}, {0},
                                        EquivalenceMap()));

  // Simple cases with one equivalence.
  EXPECT_EQ(
      std::vector<int32_t>({0}),  // {0 - 0}.
      GenerateReferencesDeltaTest(
          {{10, 3}}, {{20, 3}}, {3}, {3},
          EquivalenceMap({{{3, 3, 1}, kDummySim}, {{10, 20, 4}, kDummySim}})));
  EXPECT_EQ(
      std::vector<int32_t>({-1}),  // {0 - 1}.
      GenerateReferencesDeltaTest(
          {{10, 3}}, {{20, 3}}, {3}, {3, 4},
          EquivalenceMap({{{3, 4, 1}, kDummySim}, {{10, 20, 4}, kDummySim}})));
  EXPECT_EQ(
      std::vector<int32_t>({1}),  // {1 - 0}.
      GenerateReferencesDeltaTest(
          {{10, 3}}, {{20, 3}}, {3}, {2, 3},
          EquivalenceMap({{{3, 2, 1}, kDummySim}, {{10, 20, 4}, kDummySim}})));
  EXPECT_EQ(std::vector<int32_t>({1, -1}),  // {1 - 0, 0 - 1}.
            GenerateReferencesDeltaTest(
                {{10, 3}, {11, 4}}, {{20, 3}, {21, 4}}, {3, 4}, {2, 3, 4, 5},
                EquivalenceMap({{{3, 2, 1}, kDummySim},
                                {{4, 5, 1}, kDummySim},
                                {{10, 20, 4}, kDummySim}})));

  EXPECT_EQ(
      std::vector<int32_t>({0, 0}),  // {1 - 1, 2 - 2}.
      GenerateReferencesDeltaTest(
          {{10, 3}, {11, 4}, {12, 5}, {13, 6}},
          {{20, 3}, {21, 4}, {22, 5}, {23, 6}}, {3, 4, 5, 6}, {3, 4, 5, 6},
          EquivalenceMap({{{3, 3, 4}, kDummySim}, {{11, 21, 2}, kDummySim}})));

  // Multiple equivalences.
  EXPECT_EQ(std::vector<int32_t>({-1, 1}),  // {0 - 1, 1 - 0}.
            GenerateReferencesDeltaTest(
                {{10, 0}, {12, 1}}, {{10, 0}, {12, 1}}, {0, 1}, {0, 1},
                EquivalenceMap({{{0, 0, 2}, kDummySim},
                                {{12, 10, 2}, kDummySim},
                                {{10, 12, 2}, kDummySim}})));
  EXPECT_EQ(
      std::vector<int32_t>({0, 0}),  // {0 - 0, 1 - 1}.
      GenerateReferencesDeltaTest(
          {{0, 0}, {2, 2}}, {{0, 0}, {2, 2}}, {0, 2}, {0, 2},
          EquivalenceMap({{{2, 0, 2}, kDummySim}, {{0, 2, 2}, kDummySim}})));

  EXPECT_EQ(std::vector<int32_t>({-2, 2}),  // {0 - 2, 2 - 0}.
            GenerateReferencesDeltaTest(
                {{10, 0}, {12, 1}, {14, 2}}, {{10, 0}, {12, 1}, {14, 2}},
                {0, 1, 2}, {0, 1, 2},
                EquivalenceMap({{{0, 0, 3}, kDummySim},
                                {{14, 10, 2}, kDummySim},
                                {{10, 14, 2}, kDummySim}})));

  EXPECT_EQ(std::vector<int32_t>({-2, 2}),  // {0 - 2, 2 - 0}.
            GenerateReferencesDeltaTest(
                {{11, 0}, {14, 1}, {17, 2}}, {{11, 0}, {14, 1}, {17, 2}},
                {0, 1, 2}, {0, 1, 2},
                EquivalenceMap({{{0, 0, 3}, kDummySim},
                                {{16, 10, 3}, kDummySim},
                                {{10, 16, 3}, kDummySim}})));

  EXPECT_EQ(
      std::vector<int32_t>({-2, 2}),  // {0 - 2, 2 - 0}.
      GenerateReferencesDeltaTest({{10, 0}, {14, 2}, {16, 1}},
                                  {{10, 0}, {14, 2}}, {0, 1, 2}, {0, 1, 2},
                                  EquivalenceMap({{{0, 0, 3}, kDummySim},
                                                  {{14, 10, 2}, kDummySim},
                                                  {{12, 12, 2}, kDummySim},
                                                  {{10, 14, 2}, kDummySim}})));
}

// TODO(huangs): Add more tests.

}  // namespace zucchini
