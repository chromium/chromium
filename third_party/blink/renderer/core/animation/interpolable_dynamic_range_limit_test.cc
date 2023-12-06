// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_dynamic_range_limit.h"
#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/memory/values_equivalent.h"
#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

TEST(InterpolableDynamicRangeLimitTest, SimpleEndpointsInterpolation) {
  ScopedCSSDynamicRangeLimitForTest scoped_feature(true);
  DynamicRangeLimit limit1(cc::PaintFlags::DynamicRangeLimit::kStandard);
  DynamicRangeLimit limit2(cc::PaintFlags::DynamicRangeLimit::kHigh);

  InterpolableDynamicRangeLimit* interpolable_limit_from =
      InterpolableDynamicRangeLimit::Create(limit1);
  InterpolableDynamicRangeLimit* interpolable_limit_to =
      InterpolableDynamicRangeLimit::Create(limit2);

  InterpolableValue* interpolable_value =
      interpolable_limit_from->CloneAndZero();
  interpolable_limit_from->Interpolate(*interpolable_limit_to, 0.3,
                                       *interpolable_value);
  const auto& result_limit =
      To<InterpolableDynamicRangeLimit>(*interpolable_value);
  DynamicRangeLimit limit = result_limit.GetDynamicRangeLimit();

  EXPECT_FLOAT_EQ(limit.standard_mix, 0.7f);
  EXPECT_FLOAT_EQ(limit.constrained_high_mix, 0.f);
}

TEST(InterpolableDynamicRangeLimitTest, NestedEndpointsInterpolation) {
  ScopedCSSDynamicRangeLimitForTest scoped_feature(true);
  DynamicRangeLimit limit1(cc::PaintFlags::DynamicRangeLimit::kStandard);
  DynamicRangeLimit limit2(/*standard_mix=*/0.f, /*constrained_high_mix=*/0.3f);

  InterpolableDynamicRangeLimit* interpolable_limit_from =
      InterpolableDynamicRangeLimit::Create(limit1);
  InterpolableDynamicRangeLimit* interpolable_limit_to =
      InterpolableDynamicRangeLimit::Create(limit2);

  InterpolableValue* interpolable_value =
      interpolable_limit_from->CloneAndZero();
  interpolable_limit_from->Interpolate(*interpolable_limit_to, 0.5,
                                       *interpolable_value);
  const auto& result_limit =
      To<InterpolableDynamicRangeLimit>(*interpolable_value);
  DynamicRangeLimit limit = result_limit.GetDynamicRangeLimit();

  EXPECT_FLOAT_EQ(limit.standard_mix, .5f);
  EXPECT_FLOAT_EQ(limit.constrained_high_mix, .15f);
}

// Scale/Add should have no effect.
TEST(InterpolableDynamicRangeLimitTest, TestScaleAndAdd) {
  ScopedCSSDynamicRangeLimitForTest scoped_feature(true);
  DynamicRangeLimit limit1(/*standard_mix=*/0.3f, /*constrained_high_mix=*/0.f);
  DynamicRangeLimit limit2(cc::PaintFlags::DynamicRangeLimit::kHigh);
  InterpolableDynamicRangeLimit* interpolable_limit1 =
      InterpolableDynamicRangeLimit::Create(limit1);
  InterpolableDynamicRangeLimit* interpolable_limit2 =
      InterpolableDynamicRangeLimit::Create(limit2);

  interpolable_limit1->Scale(0.5);
  interpolable_limit1->Add(*interpolable_limit2);

  DynamicRangeLimit limit = interpolable_limit1->GetDynamicRangeLimit();

  EXPECT_EQ(limit, interpolable_limit2->GetDynamicRangeLimit());
}

TEST(InterpolableDynamicRangeLimitTest, InterpolableLimitsEqual) {
  ScopedCSSDynamicRangeLimitForTest scoped_feature(true);
  DynamicRangeLimit limit1(/*standard_mix=*/0.3f,
                           /*constrained_high_mix=*/0.f);
  DynamicRangeLimit limit2(/*standard_mix=*/0.3f,
                           /*constrained_high_mix=*/0.f);

  InterpolableDynamicRangeLimit* interpolable_limit1 =
      InterpolableDynamicRangeLimit::Create(limit1);
  InterpolableDynamicRangeLimit* interpolable_limit2 =
      InterpolableDynamicRangeLimit::Create(limit2);

  EXPECT_TRUE(interpolable_limit1->Equals(*interpolable_limit2));
  EXPECT_TRUE(interpolable_limit2->Equals(*interpolable_limit1));
}

TEST(InterpolableDynamicRangeLimitTest, InterpolableLimitsNotEqual) {
  ScopedCSSDynamicRangeLimitForTest scoped_feature(true);
  DynamicRangeLimit limit1(/*standard_mix=*/0.3f,
                           /*constrained_high_mix=*/0.f);
  DynamicRangeLimit limit2(/*standard_mix=*/0.3f,
                           /*constrained_high_mix=*/0.7f);

  InterpolableDynamicRangeLimit* interpolable_limit1 =
      InterpolableDynamicRangeLimit::Create(limit1);
  InterpolableDynamicRangeLimit* interpolable_limit2 =
      InterpolableDynamicRangeLimit::Create(limit2);

  EXPECT_FALSE(interpolable_limit1->Equals(*interpolable_limit2));
  EXPECT_FALSE(interpolable_limit2->Equals(*interpolable_limit1));
}

}  // namespace
}  // namespace blink
