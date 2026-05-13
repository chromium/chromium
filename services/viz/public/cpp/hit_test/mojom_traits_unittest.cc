// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <vector>

#include "base/compiler_specific.h"
#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/viz/public/cpp/hit_test/aggregated_hit_test_region_mojom_traits.h"
#include "services/viz/public/cpp/hit_test/hit_test_region_list_mojom_traits.h"
#include "services/viz/public/mojom/hit_test/aggregated_hit_test_region.mojom.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace viz {
namespace {

auto AnyFrameSinkId() {
  return fuzztest::ConstructorOf<FrameSinkId>(fuzztest::Arbitrary<uint32_t>(),
                                              fuzztest::Arbitrary<uint32_t>());
}

auto AnyRect() {
  return fuzztest::ConstructorOf<gfx::Rect>(
      fuzztest::Arbitrary<int>(), fuzztest::Arbitrary<int>(),
      fuzztest::InRange(0, 10000), fuzztest::InRange(0, 10000));
}

auto AnyTransform() {
  return fuzztest::ConstructorOf<gfx::Transform>();
}

auto AnyAggregatedHitTestRegion() {
  return fuzztest::Map(
      [](const FrameSinkId& frame_sink_id, uint32_t flags,
         uint32_t async_hit_test_reasons, const gfx::Rect& rect,
         const gfx::Transform& transform, int32_t child_count) {
        if (flags & HitTestRegionFlags::kHitTestAsk) {
          if (!async_hit_test_reasons) {
            async_hit_test_reasons = 1;
          }
        } else {
          async_hit_test_reasons = 0;
        }
        return AggregatedHitTestRegion(frame_sink_id, flags, rect, transform,
                                       child_count, async_hit_test_reasons);
      },
      AnyFrameSinkId(), fuzztest::Arbitrary<uint32_t>(),
      fuzztest::Arbitrary<uint32_t>(), AnyRect(), AnyTransform(),
      fuzztest::Arbitrary<int32_t>());
}

auto AnyHitTestRegion() {
  return fuzztest::Map(
      [](const FrameSinkId& frame_sink_id, uint32_t flags,
         uint32_t async_hit_test_reasons, const gfx::Rect& rect,
         const gfx::Transform& transform) {
        HitTestRegion region;
        region.frame_sink_id = frame_sink_id;
        region.flags = flags;
        region.async_hit_test_reasons = async_hit_test_reasons;
        region.rect = rect;
        region.transform = transform;
        return region;
      },
      AnyFrameSinkId(), fuzztest::Arbitrary<uint32_t>(),
      fuzztest::Arbitrary<uint32_t>(), AnyRect(), AnyTransform());
}

auto AnyHitTestRegionList() {
  return fuzztest::Map(
      [](uint32_t flags, uint32_t async_hit_test_reasons,
         const gfx::Rect& bounds, const gfx::Transform& transform,
         const std::vector<HitTestRegion>& regions) {
        HitTestRegionList list;
        list.flags = flags;
        list.async_hit_test_reasons = async_hit_test_reasons;
        list.bounds = bounds;
        list.transform = transform;
        list.regions = regions;
        return list;
      },
      fuzztest::Arbitrary<uint32_t>(), fuzztest::Arbitrary<uint32_t>(),
      AnyRect(), AnyTransform(),
      fuzztest::VectorOf(AnyHitTestRegion()).WithMaxSize(100));
}

}  // namespace

void AggregatedHitTestRegionFuzz(const AggregatedHitTestRegion& input) {
  AggregatedHitTestRegion output;
  mojo::test::SerializeAndDeserialize<mojom::AggregatedHitTestRegion>(input,
                                                                      output);
}
FUZZ_TEST(StructTraitsTest, AggregatedHitTestRegionFuzz)
    .WithDomains(AnyAggregatedHitTestRegion());

void HitTestRegionListFuzz(const HitTestRegionList& input) {
  HitTestRegionList output;
  mojo::test::SerializeAndDeserialize<mojom::HitTestRegionList>(input, output);
}
FUZZ_TEST(StructTraitsTest, HitTestRegionListFuzz)
    .WithDomains(AnyHitTestRegionList());

TEST(StructTraitsTest, AggregatedHitTestRegion) {
  constexpr FrameSinkId frame_sink_id(1337, 1234);
  constexpr uint32_t flags = HitTestRegionFlags::kHitTestAsk;
  constexpr uint32_t async_hit_test_reasons =
      AsyncHitTestReasons::kOverlappedRegion;
  constexpr gfx::Rect rect(1024, 768);
  gfx::Transform transform;
  transform.Scale(.5f, .7f);
  constexpr int32_t child_count = 5;
  AggregatedHitTestRegion input(frame_sink_id, flags, rect, transform,
                                child_count, async_hit_test_reasons);
  AggregatedHitTestRegion output;
  mojo::test::SerializeAndDeserialize<mojom::AggregatedHitTestRegion>(input,
                                                                      output);
  EXPECT_EQ(input.frame_sink_id, output.frame_sink_id);
  EXPECT_EQ(input.flags, output.flags);
  EXPECT_EQ(input.async_hit_test_reasons, output.async_hit_test_reasons);
  EXPECT_EQ(input.rect, output.rect);
  EXPECT_EQ(input.transform, output.transform);
  EXPECT_EQ(input.child_count, output.child_count);
}

TEST(StructTraitsTest, HitTestRegionList) {
  std::optional<HitTestRegionList> input(std::in_place);
  input->flags = HitTestRegionFlags::kHitTestAsk;
  input->async_hit_test_reasons = AsyncHitTestReasons::kOverlappedRegion;
  input->bounds = gfx::Rect(1, 2, 3, 4);
  input->transform.Scale(0.5f, 0.7f);

  HitTestRegion input_region1;
  input_region1.flags = HitTestRegionFlags::kHitTestIgnore;
  input_region1.async_hit_test_reasons = AsyncHitTestReasons::kNotAsyncHitTest;
  input_region1.frame_sink_id = FrameSinkId(12, 13);
  input_region1.rect = gfx::Rect(4, 5, 6, 7);
  input_region1.transform.Scale(1.2f, 1.3f);
  input->regions.push_back(input_region1);

  std::optional<HitTestRegionList> output;
  mojo::test::SerializeAndDeserialize<mojom::HitTestRegionList>(input, output);
  EXPECT_TRUE(output);
  EXPECT_EQ(input->flags, output->flags);
  EXPECT_EQ(input->async_hit_test_reasons, output->async_hit_test_reasons);
  EXPECT_EQ(input->bounds, output->bounds);
  EXPECT_EQ(input->transform, output->transform);
  EXPECT_EQ(input->regions.size(), output->regions.size());
  EXPECT_EQ(input->regions[0].flags, output->regions[0].flags);
  EXPECT_EQ(input->regions[0].async_hit_test_reasons,
            output->regions[0].async_hit_test_reasons);
  EXPECT_EQ(input->regions[0].frame_sink_id, output->regions[0].frame_sink_id);
  EXPECT_EQ(input->regions[0].rect, output->regions[0].rect);
  EXPECT_EQ(input->regions[0].transform, output->regions[0].transform);
}

// Ensures gfx::Transform doesn't mutate itself when its const methods are
// called, to ensure it won't change in the read-only shared memory segment.
TEST(StructTraitsTest, TransformImmutable) {
  auto t = gfx::Transform::RowMajor(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                    14, 15, 16);
  uint8_t mem[sizeof(t)];
  UNSAFE_TODO(std::memcpy(&mem, &t, sizeof(t)));
  EXPECT_FALSE(t.IsIdentity());
  UNSAFE_TODO(EXPECT_EQ(0, std::memcmp(&t, &mem, sizeof(t))));
}

}  // namespace viz
