// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolation_effect.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

double GetInterpolableNumber(Interpolation* value) {
  auto* interpolation = To<TransitionInterpolation>(value);
  TypedInterpolationValue* interpolated_value =
      interpolation->GetInterpolatedValue();
  return To<InterpolableNumber>(interpolated_value->GetInterpolableValue())
      .Value(CSSToLengthConversionData());
}

Interpolation* CreateInterpolation(int from, int to) {
  // We require a property that maps to CSSNumberInterpolationType. 'z-index'
  // suffices for this, and also means we can ignore the AnimatableValues for
  // the compositor (as z-index isn't compositor-compatible).
  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType interpolation_type(property_handle);
  InterpolationValue start(MakeGarbageCollected<InterpolableNumber>(from));
  InterpolationValue end(MakeGarbageCollected<InterpolableNumber>(to));
  return MakeGarbageCollected<TransitionInterpolation>(
      property_handle, interpolation_type, std::move(start), std::move(end),
      nullptr, nullptr);
}

}  // namespace

TEST(AnimationInterpolationEffectTest, SingleInterpolation) {
  test::TaskEnvironment task_environment;
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  interpolation_effect->AddInterpolation(
      CreateInterpolation(0, 10), scoped_refptr<TimingFunction>(), 0, 1, -1, 2);

  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      -2, TimingFunction::LimitDirection::LEFT, active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect->GetActiveInterpolations(
      -0.5, TimingFunction::LimitDirection::LEFT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_EQ(-5, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0.5, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(5, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      1.5, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(15, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      3, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect->GetActiveInterpolations(
      0, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
}

TEST(AnimationInterpolationEffectTest, MultipleInterpolations) {
  test::TaskEnvironment task_environment;
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  interpolation_effect->AddInterpolation(
      CreateInterpolation(10, 15), scoped_refptr<TimingFunction>(), 1, 2, 1, 3);
  interpolation_effect->AddInterpolation(
      CreateInterpolation(0, 1), LinearTimingFunction::Shared(), 0, 1, 0, 1);
  interpolation_effect->AddInterpolation(
      CreateInterpolation(1, 6),
      CubicBezierTimingFunction::Preset(
          CubicBezierTimingFunction::EaseType::EASE),
      0.5, 1.5, 0.5, 1.5);

  // ease = cubicBezier(0.25, 0.1, 0.25, 1)
  // ease(0.5) = 0.8024033877399112

  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      -0.5, TimingFunction::LimitDirection::LEFT, active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect->GetActiveInterpolations(
      0, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(0, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0.5, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(0.5f, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(1, GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect->GetActiveInterpolations(
      1, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(10, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(5.0120169f,
                  GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect->GetActiveInterpolations(
      1.5, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(12.5f, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      2, TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(15, GetInterpolableNumber(active_interpolations.at(0)));
}

}  // namespace blink
