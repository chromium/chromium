// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolation_effect.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_transform_list.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

double GetInterpolableNumber(Interpolation* value) {
  auto* interpolation = To<TransitionInterpolation>(value);
  TypedInterpolationValue* interpolated_value =
      interpolation->GetInterpolatedValue();
  return To<InterpolableNumber>(interpolated_value->GetInterpolableValue())
      .Value(CSSToLengthConversionData(/*element=*/nullptr));
}

Interpolation* CreateInterpolation(int from, int to) {
  // We require a property that maps to CSSNumberInterpolationType. 'z-index'
  // suffices for this, and also means we can ignore the AnimatableValues for
  // the compositor (as z-index isn't compositor-compatible).
  PropertyHandle property_handle(GetCSSPropertyZIndex());
  CSSNumberInterpolationType* interpolation_type(
      MakeGarbageCollected<CSSNumberInterpolationType>(property_handle));
  InterpolationValue start(MakeGarbageCollected<InterpolableNumber>(from));
  InterpolationValue end(MakeGarbageCollected<InterpolableNumber>(to));
  return MakeGarbageCollected<TransitionInterpolation>(
      property_handle, interpolation_type, std::move(start), std::move(end),
      nullptr, nullptr);
}

// Helper class for tests that use InvalidatableInterpolation
class InterpolationEffectTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    element_ = GetDocument().CreateElementForBinding(AtomicString("div"));
  }

  // Creates an InvalidatableInterpolation for testing iteration composite
  Interpolation* CreateInvalidatableInterpolation(CSSPropertyID property,
                                                  const String& from,
                                                  const String& to) {
    PropertyHandle property_handle(CSSProperty::Get(property));

    Keyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetOffset(0.0);
    To<StringKeyframe>(start_keyframe)
        ->SetCSSPropertyValue(property, from,
                              SecureContextMode::kInsecureContext, nullptr);

    Keyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetOffset(1.0);
    To<StringKeyframe>(end_keyframe)
        ->SetCSSPropertyValue(property, to, SecureContextMode::kInsecureContext,
                              nullptr);

    PropertySpecificKeyframe* from_keyframe =
        start_keyframe->CreatePropertySpecificKeyframe(
            property_handle, EffectModel::kCompositeReplace, 0.0);
    PropertySpecificKeyframe* to_keyframe =
        end_keyframe->CreatePropertySpecificKeyframe(
            property_handle, EffectModel::kCompositeReplace, 1.0);

    return MakeGarbageCollected<InvalidatableInterpolation>(
        property_handle, from_keyframe, to_keyframe);
  }

  enum class Segment { kFirst, kSecond };

  // Creates an InvalidatableInterpolation with three keyframes (0.0, 0.5, 1.0).
  // Returns the interpolation for the specified segment of the animation.
  Interpolation* CreateMultiKeyframeInterpolation(CSSPropertyID property,
                                                  const String& from,
                                                  const String& mid,
                                                  const String& to,
                                                  Segment segment) {
    PropertyHandle property_handle(CSSProperty::Get(property));

    Keyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetOffset(0.0);
    To<StringKeyframe>(start_keyframe)
        ->SetCSSPropertyValue(property, from,
                              SecureContextMode::kInsecureContext, nullptr);

    Keyframe* mid_keyframe = MakeGarbageCollected<StringKeyframe>();
    mid_keyframe->SetOffset(0.5);
    To<StringKeyframe>(mid_keyframe)
        ->SetCSSPropertyValue(property, mid,
                              SecureContextMode::kInsecureContext, nullptr);

    Keyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetOffset(1.0);
    To<StringKeyframe>(end_keyframe)
        ->SetCSSPropertyValue(property, to, SecureContextMode::kInsecureContext,
                              nullptr);

    PropertySpecificKeyframe* from_ps_keyframe =
        start_keyframe->CreatePropertySpecificKeyframe(
            property_handle, EffectModel::kCompositeReplace, 0.0);
    PropertySpecificKeyframe* mid_ps_keyframe =
        mid_keyframe->CreatePropertySpecificKeyframe(
            property_handle, EffectModel::kCompositeReplace, 0.5);
    PropertySpecificKeyframe* to_ps_keyframe =
        end_keyframe->CreatePropertySpecificKeyframe(
            property_handle, EffectModel::kCompositeReplace, 1.0);

    if (segment == Segment::kFirst) {
      return MakeGarbageCollected<InvalidatableInterpolation>(
          property_handle, from_ps_keyframe, mid_ps_keyframe, to_ps_keyframe);
    } else {
      return MakeGarbageCollected<InvalidatableInterpolation>(
          property_handle, mid_ps_keyframe, to_ps_keyframe, to_ps_keyframe);
    }
  }

  // Gets the numeric value from an InvalidatableInterpolation
  double GetInvalidatableNumber(Interpolation* interpolation) {
    auto* invalidatable = To<InvalidatableInterpolation>(interpolation);

    // Ensure the value is cached by applying the interpolation
    ActiveInterpolations* interpolations =
        MakeGarbageCollected<ActiveInterpolations>();
    interpolations->push_back(invalidatable);
    animation_test_helpers::EnsureInterpolatedValueCached(
        interpolations, GetDocument(), element_);

    const TypedInterpolationValue* typed_value =
        invalidatable->GetCachedValueForTesting();
    EXPECT_NE(nullptr, typed_value);
    if (!typed_value) {
      return 0.0;
    }

    return To<InterpolableNumber>(typed_value->GetInterpolableValue())
        .Value(CSSToLengthConversionData(/*element=*/nullptr));
  }

  // Gets the transform matrix from an InvalidatableInterpolation
  gfx::Transform GetInvalidatableTransform(Interpolation* interpolation) {
    auto* invalidatable = To<InvalidatableInterpolation>(interpolation);

    ActiveInterpolations* interpolations =
        MakeGarbageCollected<ActiveInterpolations>();
    interpolations->push_back(invalidatable);
    animation_test_helpers::EnsureInterpolatedValueCached(
        interpolations, GetDocument(), element_);

    const TypedInterpolationValue* typed_value =
        invalidatable->GetCachedValueForTesting();
    EXPECT_NE(nullptr, typed_value);
    if (!typed_value) {
      return gfx::Transform();
    }

    const auto& transform_list =
        To<InterpolableTransformList>(typed_value->GetInterpolableValue());
    gfx::Transform result;
    transform_list.operations().Apply(gfx::SizeF(), result);
    return result;
  }

 protected:
  Persistent<Element> element_;
};

}  // namespace

TEST(AnimationInterpolationEffectTest, SingleInterpolation) {
  test::TaskEnvironment task_environment;
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  interpolation_effect->AddInterpolation(
      CreateInterpolation(0, 10), scoped_refptr<TimingFunction>(), 0, 1, -1, 2);

  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      0, -2, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::LEFT, active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect->GetActiveInterpolations(
      0, -0.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::LEFT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_EQ(-5, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0, 0.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(5, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0, 1.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(15, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0, 3, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect->GetActiveInterpolations(
      0, 0, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
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
      0, -0.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::LEFT, active_interpolations);
  EXPECT_EQ(0ul, active_interpolations.size());

  interpolation_effect->GetActiveInterpolations(
      0, 0, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(0, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0, 0.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(0.5f, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(1, GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect->GetActiveInterpolations(
      0, 1, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(2ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(10, GetInterpolableNumber(active_interpolations.at(0)));
  EXPECT_FLOAT_EQ(5.0120169f,
                  GetInterpolableNumber(active_interpolations.at(1)));

  interpolation_effect->GetActiveInterpolations(
      0, 1.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(12.5f, GetInterpolableNumber(active_interpolations.at(0)));

  interpolation_effect->GetActiveInterpolations(
      0, 2, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_FLOAT_EQ(15, GetInterpolableNumber(active_interpolations.at(0)));
}

// Tests for iterationComposite behavior with InvalidatableInterpolation

// Replace mode doesn't accumulate across iterations
TEST_F(InterpolationEffectTest, IterationCompositeReplaceValue) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation =
      CreateInvalidatableInterpolation(CSSPropertyID::kZIndex, "0", "100");

  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      1, 0.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);

  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_NEAR(50, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);
}

// Accumulate mode: iteration * end_value + interpolated_value
TEST_F(InterpolationEffectTest, IterationCompositeAccumulateValue) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation =
      CreateInvalidatableInterpolation(CSSPropertyID::kZIndex, "0", "100");

  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      1, 0.5, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);

  EXPECT_EQ(1ul, active_interpolations.size());
  // 1 * 100 + 50 = 150
  EXPECT_NEAR(150, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);
}

// Accumulate works with reverse direction
TEST_F(InterpolationEffectTest, IterationCompositeAccumulateReverse) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation =
      CreateInvalidatableInterpolation(CSSPropertyID::kZIndex, "0", "100");

  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  HeapVector<Member<Interpolation>> active_interpolations;
  // At iteration 1, fraction 0.0: 1 * 100 + 0 = 100
  interpolation_effect->GetActiveInterpolations(
      1, 0.0, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);

  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_NEAR(100, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);
}

// Switching between accumulate and replace modes works correctly
TEST_F(InterpolationEffectTest, IterationCompositeMutation) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation =
      CreateInvalidatableInterpolation(CSSPropertyID::kZIndex, "0", "100");

  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  HeapVector<Member<Interpolation>> active_interpolations;

  interpolation_effect->GetActiveInterpolations(
      1, 0.5, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_NEAR(150, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);

  active_interpolations.clear();

  interpolation_effect->GetActiveInterpolations(
      1, 0.5, EffectModel::kIterationCompositeReplace,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_NEAR(50, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);

  active_interpolations.clear();

  interpolation_effect->GetActiveInterpolations(
      1, 0.5, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_NEAR(150, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);
}

// Accumulate works with non-zero start values
TEST_F(InterpolationEffectTest, IterationCompositeAccumulateNonZeroStart) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation =
      CreateInvalidatableInterpolation(CSSPropertyID::kZIndex, "100", "200");

  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  HeapVector<Member<Interpolation>> active_interpolations;
  // At iteration 2, fraction 0.0: 2 * 200 + 100 = 500
  interpolation_effect->GetActiveInterpolations(
      2, 0.0, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);

  EXPECT_EQ(1ul, active_interpolations.size());
  EXPECT_NEAR(500, GetInvalidatableNumber(active_interpolations.at(0)), 0.1);
}

// Accumulated values are clamped to valid property ranges (opacity [0,1])
TEST_F(InterpolationEffectTest, IterationCompositeAccumulateClamping) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation =
      CreateInvalidatableInterpolation(CSSPropertyID::kOpacity, "0", "1");

  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      1, 0.0, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);

  EXPECT_EQ(1ul, active_interpolations.size());
  double value = GetInvalidatableNumber(active_interpolations.at(0));
  EXPECT_FLOAT_EQ(1.0, value);
  EXPECT_LE(value, 1.0);
}

// Test accumulation through several iterations with multiple keyframes.
TEST_F(InterpolationEffectTest, IterationCompositeAccumulateMultiKeyframe) {
  Interpolation* first_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kZIndex, "0", "50", "100", Segment::kFirst);
  Interpolation* second_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kZIndex, "0", "50", "100", Segment::kSecond);

  // Iteration 0: no accumulation
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(25, GetInvalidatableNumber(result.at(0)));
  }
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(75, GetInvalidatableNumber(result.at(0)));
  }

  // Iteration 1: accumulate 1 * 100
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        1, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(125, GetInvalidatableNumber(result.at(0)));  // 25 + 100
  }
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        1, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(175, GetInvalidatableNumber(result.at(0)));  // 75 + 100
  }

  // Iteration 3: accumulate 3 * 100
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        3, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(325, GetInvalidatableNumber(result.at(0)));  // 25 + 300
  }
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        3, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(375, GetInvalidatableNumber(result.at(0)));  // 75 + 300
  }
}

// Verify that replace mode does not accumulate regardless of iteration.
TEST_F(InterpolationEffectTest, IterationCompositeReplaceMultiKeyframe) {
  Interpolation* first_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kZIndex, "0", "50", "100", Segment::kFirst);
  Interpolation* second_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kZIndex, "0", "50", "100", Segment::kSecond);

  // First segment at iterations 0, 1, 3
  for (int iteration : {0, 1, 3}) {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        iteration, 0.25, EffectModel::kIterationCompositeReplace,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(25, GetInvalidatableNumber(result.at(0)));
  }

  // Second segment at iterations 0, 1, 3
  for (int iteration : {0, 1, 3}) {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        iteration, 0.75, EffectModel::kIterationCompositeReplace,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(75, GetInvalidatableNumber(result.at(0)));
  }
}

// Iteration accumulation skips incompatible (IACVT) lengths without crashing.
// See: https://crbug.com/467366440
TEST_F(InterpolationEffectTest,
       IterationCompositeAccumulateIncompatibleLengths) {
  Persistent<InterpolationEffect> interpolation_effect =
      MakeGarbageCollected<InterpolationEffect>();
  Interpolation* interpolation = CreateInvalidatableInterpolation(
      CSSPropertyID::kWidth, "auto", "fit-content");
  interpolation_effect->AddInterpolation(
      interpolation, scoped_refptr<TimingFunction>(), 0, 1, 0, 1);

  // Test at iteration 1, fraction 0.4.
  HeapVector<Member<Interpolation>> active_interpolations;
  interpolation_effect->GetActiveInterpolations(
      1, 0.4, EffectModel::kIterationCompositeAccumulate,
      TimingFunction::LimitDirection::RIGHT, active_interpolations);
  EXPECT_EQ(1ul, active_interpolations.size());

  auto* invalidatable =
      To<InvalidatableInterpolation>(active_interpolations.at(0).Get());
  ActiveInterpolations* interpolations =
      MakeGarbageCollected<ActiveInterpolations>();
  interpolations->push_back(invalidatable);
  animation_test_helpers::EnsureInterpolatedValueCached(
      interpolations, GetDocument(), element_);
  EXPECT_NE(nullptr, invalidatable->GetCachedValueForTesting());
}

// Test transform accumulation with scale() over 2 keyframes.
TEST_F(InterpolationEffectTest, IterationCompositeAccumulateTransform) {
  Interpolation* interpolation = CreateInvalidatableInterpolation(
      CSSPropertyID::kTransform, "scale(1)", "scale(3)");

  // Iteration 0 at 0.5. scale(1) to scale(3) at 50%: scale(2).
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(interpolation, scoped_refptr<TimingFunction>(), 0,
                             1, 0, 1);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.5, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeScale(2),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 2 at 0.0. scale(1 + (3-1)*2) = scale(5).
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(interpolation, scoped_refptr<TimingFunction>(), 0,
                             1, 0, 1);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        2, 0.0, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeScale(5),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 2 at 0.5. scale(5) to scale(7) at 50%: scale(6).
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(interpolation, scoped_refptr<TimingFunction>(), 0,
                             1, 0, 1);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        2, 0.5, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeScale(6),
              GetInvalidatableTransform(result.at(0)));
  }
}

// Test transform accumulation with translateX() over 3 keyframes.
TEST_F(InterpolationEffectTest,
       IterationCompositeAccumulateTransformMultiKeyframe) {
  Interpolation* first_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kTransform, "translateX(0px)", "translateX(30px)",
      "translateX(100px)", Segment::kFirst);
  Interpolation* second_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kTransform, "translateX(0px)", "translateX(30px)",
      "translateX(100px)", Segment::kSecond);

  // Iteration 0 at 0.25. First segment, 0px to 30px at 50%: 15px.
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeTranslation(15, 0),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 2 at 0.25. First segment, 200px to 230px at 50%: 215px.
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        2, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeTranslation(215, 0),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 0 at 0.75. Second segment, 30px to 100px at 50%: 65px.
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeTranslation(65, 0),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 2 at 0.75. Second segment, 230px to 300px at 50%: 265px.
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        2, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::MakeTranslation(265, 0),
              GetInvalidatableTransform(result.at(0)));
  }
}

// Test transform accumulation with translateX() and scale() over 3 keyframes.
// Translate accumulates additively while scale uses one-based addition.
TEST_F(InterpolationEffectTest,
       IterationCompositeAccumulateTransformListMultiKeyframe) {
  Interpolation* first_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kTransform, "translateX(0px) scale(1)",
      "translateX(4px) scale(3)", "translateX(10px) scale(5)", Segment::kFirst);
  Interpolation* second_segment = CreateMultiKeyframeInterpolation(
      CSSPropertyID::kTransform, "translateX(0px) scale(1)",
      "translateX(4px) scale(3)", "translateX(10px) scale(5)",
      Segment::kSecond);

  // Iteration 0 at 0.25. First segment at 50%: translateX(2px) scale(2).
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::Affine(2, 0, 0, 2, 2, 0),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 2 at 0.25. Accumulate delta translateX(10px) scale(5) twice:
  //   translate: 0+20, 4+20 at 50% = 22. scale: 1+8, 3+8 at 50% = 10.
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(first_segment, scoped_refptr<TimingFunction>(), 0,
                             0.5, 0, 0.5);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        2, 0.25, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::Affine(10, 0, 0, 10, 22, 0),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 0 at 0.75. Second segment at 50%: translateX(7px) scale(4).
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        0, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::Affine(4, 0, 0, 4, 7, 0),
              GetInvalidatableTransform(result.at(0)));
  }

  // Iteration 2 at 0.75. Accumulate delta twice:
  //   translate: 4+20, 10+20 at 50% = 27. scale: 3+8, 5+8 at 50% = 12.
  {
    Persistent<InterpolationEffect> effect =
        MakeGarbageCollected<InterpolationEffect>();
    effect->AddInterpolation(second_segment, scoped_refptr<TimingFunction>(),
                             0.5, 1.0, 0.5, 1.0);
    HeapVector<Member<Interpolation>> result;
    effect->GetActiveInterpolations(
        2, 0.75, EffectModel::kIterationCompositeAccumulate,
        TimingFunction::LimitDirection::RIGHT, result);
    EXPECT_EQ(gfx::Transform::Affine(12, 0, 0, 12, 27, 0),
              GetInvalidatableTransform(result.at(0)));
  }
}

}  // namespace blink
