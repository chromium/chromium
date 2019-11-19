// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/aggregated_hit_test_region.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/viz/public/cpp/hit_test/aggregated_hit_test_region_mojom_traits.h"
#include "services/viz/public/cpp/hit_test/hit_test_region_list_mojom_traits.h"
#include "services/viz/public/mojom/hit_test/aggregated_hit_test_region.mojom.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/transform_mojom_traits.h"

namespace viz {

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
  mojo::test::SerializeAndDeserialize<mojom::AggregatedHitTestRegion>(&input,
                                                                      &output);
  EXPECT_EQ(input.frame_sink_id, output.frame_sink_id);
  EXPECT_EQ(input.flags, output.flags);
  EXPECT_EQ(input.async_hit_test_reasons, output.async_hit_test_reasons);
  EXPECT_EQ(input.rect, output.rect);
  EXPECT_EQ(input.transform(), output.transform());
  EXPECT_EQ(input.child_count, output.child_count);
}

TEST(StructTraitsTest, HitTestRegionList) {
  base::Optional<HitTestRegionList> input(base::in_place);
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

  base::Optional<HitTestRegionList> output;
  mojo::test::SerializeAndDeserialize<mojom::HitTestRegionList>(&input,
                                                                &output);
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

}  // namespace viz
