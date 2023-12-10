// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/inert_effect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using animation_test_helpers::CreateSimpleKeyframeEffectModelForTest;

TEST(InertEffectTest, IsCurrent) {
  test::TaskEnvironment task_environment;
  auto* opacity_model =
      CreateSimpleKeyframeEffectModelForTest(CSSPropertyID::kOpacity, "0", "1");

  {
    Timing timing;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1000);

    auto* inert_effect = MakeGarbageCollected<InertEffect>(
        opacity_model, timing, animation_test_helpers::TestAnimationProxy());
    HeapVector<Member<Interpolation>> interpolations;
    // Calling Sample ensures Timing is calculated.
    inert_effect->Sample(interpolations);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_TRUE(inert_effect->IsCurrent());
  }

  {
    Timing timing;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1000);
    timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(500));

    auto* inert_effect = MakeGarbageCollected<InertEffect>(
        opacity_model, timing, animation_test_helpers::TestAnimationProxy());
    HeapVector<Member<Interpolation>> interpolations;
    // Calling Sample ensures Timing is calculated.
    inert_effect->Sample(interpolations);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_TRUE(inert_effect->IsCurrent());
  }

  {
    Timing timing;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1000);
    timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(500));

    animation_test_helpers::TestAnimationProxy proxy;
    proxy.SetPlaybackRate(-1);

    auto* inert_effect =
        MakeGarbageCollected<InertEffect>(opacity_model, timing, proxy);

    HeapVector<Member<Interpolation>> interpolations;
    // Calling Sample ensures Timing is calculated.
    inert_effect->Sample(interpolations);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_FALSE(inert_effect->IsCurrent());
  }
}

TEST(InertEffectTest, Affects) {
  test::TaskEnvironment task_environment;
  auto* opacity_model =
      CreateSimpleKeyframeEffectModelForTest(CSSPropertyID::kOpacity, "0", "1");
  auto* color_model = CreateSimpleKeyframeEffectModelForTest(
      CSSPropertyID::kColor, "red", "green");

  Timing timing;

  auto* opacity_effect = MakeGarbageCollected<InertEffect>(
      opacity_model, timing, animation_test_helpers::TestAnimationProxy());

  auto* color_effect = MakeGarbageCollected<InertEffect>(
      color_model, timing, animation_test_helpers::TestAnimationProxy());

  EXPECT_TRUE(opacity_effect->Affects(PropertyHandle(GetCSSPropertyOpacity())));
  EXPECT_FALSE(opacity_effect->Affects(PropertyHandle(GetCSSPropertyColor())));

  EXPECT_TRUE(color_effect->Affects(PropertyHandle(GetCSSPropertyColor())));
  EXPECT_FALSE(color_effect->Affects(PropertyHandle(GetCSSPropertyOpacity())));
}

}  // namespace blink
