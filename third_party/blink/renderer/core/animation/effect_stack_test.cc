// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/effect_stack.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

using animation_test_helpers::EnsureInterpolatedValueCached;

class AnimationEffectStackTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    GetDocument().GetAnimationClock().ResetTimeForTesting();
    timeline = GetDocument().Timeline();
    element = GetDocument().CreateElementForBinding(AtomicString("foo"));
  }

  Animation* Play(KeyframeEffect* effect, double start_time) {
    Animation* animation = timeline->Play(effect);
    animation->setStartTime(
        MakeGarbageCollected<V8CSSNumberish>(start_time * 1000),
        ASSERT_NO_EXCEPTION);
    animation->Update(kTimingUpdateOnDemand);
    return animation;
  }

  void UpdateTimeline(base::TimeDelta time) {
    GetDocument().GetAnimationClock().UpdateTime(
        GetDocument().Timeline().CalculateZeroTime() + time);
    timeline->ServiceAnimations(kTimingUpdateForAnimationFrame);
  }

  size_t SampledEffectCount() {
    return element->EnsureElementAnimations()
        .GetEffectStack()
        .sampled_effects_.size();
  }

  KeyframeEffectModelBase* MakeEffectModel(CSSPropertyID id,
                                           const String& value) {
    StringKeyframeVector keyframes(2);
    keyframes[0] = MakeGarbageCollected<StringKeyframe>();
    keyframes[0]->SetOffset(0.0);
    keyframes[0]->SetCSSPropertyValue(
        id, value, SecureContextMode::kInsecureContext, nullptr);
    keyframes[1] = MakeGarbageCollected<StringKeyframe>();
    keyframes[1]->SetOffset(1.0);
    keyframes[1]->SetCSSPropertyValue(
        id, value, SecureContextMode::kInsecureContext, nullptr);
    return MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  }

  InertEffect* MakeInertEffect(KeyframeEffectModelBase* effect) {
    Timing timing;
    timing.fill_mode = Timing::FillMode::BOTH;
    return MakeGarbageCollected<InertEffect>(
        effect, timing, animation_test_helpers::TestAnimationProxy());
  }

  KeyframeEffect* MakeKeyframeEffect(KeyframeEffectModelBase* effect,
                                     double duration = 10) {
    Timing timing;
    timing.fill_mode = Timing::FillMode::BOTH;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(duration);
    return MakeGarbageCollected<KeyframeEffect>(element.Get(), effect, timing);
  }

  double GetFontSizeValue(
      const ActiveInterpolationsMap& active_interpolations) {
    ActiveInterpolations* interpolations =
        active_interpolations.at(PropertyHandle(GetCSSPropertyFontSize()));
    EnsureInterpolatedValueCached(interpolations, GetDocument(), element);

    const auto* typed_value =
        To<InvalidatableInterpolation>(*interpolations->at(0))
            .GetCachedValueForTesting();
    // font-size is stored as an |InterpolableLength|; here we assume pixels.
    EXPECT_TRUE(typed_value->GetInterpolableValue().IsLength());
    const InterpolableLength& length =
        To<InterpolableLength>(typed_value->GetInterpolableValue());
    return length.CreateCSSValue(Length::ValueRange::kAll)->GetDoubleValue();
  }

  double GetZIndexValue(const ActiveInterpolationsMap& active_interpolations) {
    ActiveInterpolations* interpolations =
        active_interpolations.at(PropertyHandle(GetCSSPropertyZIndex()));
    EnsureInterpolatedValueCached(interpolations, GetDocument(), element);

    const auto* typed_value =
        To<InvalidatableInterpolation>(*interpolations->at(0))
            .GetCachedValueForTesting();
    // z-index is stored as a straight number value.
    EXPECT_TRUE(typed_value->GetInterpolableValue().IsNumber());
    return To<InterpolableNumber>(&typed_value->GetInterpolableValue())
        ->Value(CSSToLengthConversionData());
  }

  Persistent<DocumentTimeline> timeline;
  Persistent<Element> element;
};

TEST_F(AnimationEffectStackTest, ElementAnimationsSorted) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "1px")),
       10);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "2px")),
       15);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "3px")), 5);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffect::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetFontSizeValue(result), 3);
}

TEST_F(AnimationEffectStackTest, NewAnimations) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "1px")),
       15);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kZIndex, "2")), 10);
  HeapVector<Member<const InertEffect>> new_animations;
  InertEffect* inert1 =
      MakeInertEffect(MakeEffectModel(CSSPropertyID::kFontSize, "3px"));
  InertEffect* inert2 =
      MakeInertEffect(MakeEffectModel(CSSPropertyID::kZIndex, "4"));
  new_animations.push_back(inert1);
  new_animations.push_back(inert2);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), &new_animations,
      nullptr, KeyframeEffect::kDefaultPriority);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(GetFontSizeValue(result), 3);
  EXPECT_EQ(GetZIndexValue(result), 4);
}

TEST_F(AnimationEffectStackTest, CancelledAnimations) {
  HeapHashSet<Member<const Animation>> cancelled_animations;
  Animation* animation = Play(
      MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "1px")), 0);
  cancelled_animations.insert(animation);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kZIndex, "2")), 0);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr,
      &cancelled_animations, KeyframeEffect::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetZIndexValue(result), 2);
}

TEST_F(AnimationEffectStackTest, ClearedEffectsRemoved) {
  Animation* animation = Play(
      MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "1px")), 10);
  ActiveInterpolationsMap result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffect::kDefaultPriority);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(GetFontSizeValue(result), 1);

  animation->setEffect(nullptr);
  result = EffectStack::ActiveInterpolations(
      &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
      KeyframeEffect::kDefaultPriority);
  EXPECT_EQ(0u, result.size());
}

TEST_F(AnimationEffectStackTest, ForwardsFillDiscarding) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "1px")), 2);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "2px")), 6);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kFontSize, "3px")), 4);
  GetDocument().GetPendingAnimations().Update(nullptr);

  // Because we will be forcing a naive GC that assumes there are no Oilpan
  // objects on the stack (e.g. passes BlinkGC::kNoHeapPointersOnStack), we have
  // to keep the ActiveInterpolationsMap in a Persistent.
  Persistent<ActiveInterpolationsMap> interpolations;

  UpdateTimeline(base::Seconds(11));
  ThreadState::Current()->CollectAllGarbageForTesting();
  interpolations = MakeGarbageCollected<ActiveInterpolationsMap>(
      EffectStack::ActiveInterpolations(
          &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
          KeyframeEffect::kDefaultPriority));
  EXPECT_EQ(1u, interpolations->size());
  EXPECT_EQ(GetFontSizeValue(*interpolations), 3);
  EXPECT_EQ(3u, SampledEffectCount());

  UpdateTimeline(base::Seconds(13));
  ThreadState::Current()->CollectAllGarbageForTesting();
  interpolations = MakeGarbageCollected<ActiveInterpolationsMap>(
      EffectStack::ActiveInterpolations(
          &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
          KeyframeEffect::kDefaultPriority));
  EXPECT_EQ(1u, interpolations->size());
  EXPECT_EQ(GetFontSizeValue(*interpolations), 3);
  EXPECT_EQ(3u, SampledEffectCount());

  UpdateTimeline(base::Seconds(15));
  ThreadState::Current()->CollectAllGarbageForTesting();
  interpolations = MakeGarbageCollected<ActiveInterpolationsMap>(
      EffectStack::ActiveInterpolations(
          &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
          KeyframeEffect::kDefaultPriority));
  EXPECT_EQ(1u, interpolations->size());
  EXPECT_EQ(GetFontSizeValue(*interpolations), 3);
  EXPECT_EQ(2u, SampledEffectCount());

  UpdateTimeline(base::Seconds(17));
  ThreadState::Current()->CollectAllGarbageForTesting();
  interpolations = MakeGarbageCollected<ActiveInterpolationsMap>(
      EffectStack::ActiveInterpolations(
          &element->GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
          KeyframeEffect::kDefaultPriority));
  EXPECT_EQ(1u, interpolations->size());
  EXPECT_EQ(GetFontSizeValue(*interpolations), 3);
  EXPECT_EQ(1u, SampledEffectCount());
}

TEST_F(AnimationEffectStackTest, AffectsPropertiesCSSBitsetDefaultPriority) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kColor, "red")), 10);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kTop, "1px")), 10);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kLeft, "1px")), 10);

  ASSERT_TRUE(element->GetElementAnimations());
  const EffectStack& effect_stack =
      element->GetElementAnimations()->GetEffectStack();

  EXPECT_FALSE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kBackgroundColor}),
      KeyframeEffect::kDefaultPriority));
  EXPECT_FALSE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kBackgroundColor, CSSPropertyID::kFontSize}),
      KeyframeEffect::kDefaultPriority));
  EXPECT_FALSE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kColor}), KeyframeEffect::kTransitionPriority));

  EXPECT_TRUE(effect_stack.AffectsProperties(CSSBitset({CSSPropertyID::kColor}),
                                             KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(effect_stack.AffectsProperties(CSSBitset({CSSPropertyID::kTop}),
                                             KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(effect_stack.AffectsProperties(CSSBitset({CSSPropertyID::kLeft}),
                                             KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kColor, CSSPropertyID::kRight}),
      KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kColor, CSSPropertyID::kTop}),
      KeyframeEffect::kDefaultPriority));
  EXPECT_FALSE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kColor}), KeyframeEffect::kTransitionPriority));
}

TEST_F(AnimationEffectStackTest, AffectsPropertiesCSSBitsetTransitionPriority) {
  Element* body = GetDocument().body();
  body->SetInlineStyleProperty(CSSPropertyID::kTransition, "color 10s");
  body->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  UpdateAllLifecyclePhasesForTest();

  body->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(body->GetElementAnimations());
  const EffectStack& effect_stack =
      body->GetElementAnimations()->GetEffectStack();

  EXPECT_FALSE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kColor}), KeyframeEffect::kDefaultPriority));
  EXPECT_TRUE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kColor}), KeyframeEffect::kTransitionPriority));
  EXPECT_FALSE(effect_stack.AffectsProperties(
      CSSBitset({CSSPropertyID::kBackgroundColor}),
      KeyframeEffect::kTransitionPriority));
}

TEST_F(AnimationEffectStackTest, AffectedPropertiesDefaultPriority) {
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kColor, "red")), 10);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kTop, "1px")), 10);
  Play(MakeKeyframeEffect(MakeEffectModel(CSSPropertyID::kLeft, "1px")), 10);

  ASSERT_TRUE(element->GetElementAnimations());
  const EffectStack& effect_stack =
      element->GetElementAnimations()->GetEffectStack();

  EXPECT_TRUE(
      effect_stack.AffectedProperties(KeyframeEffect::kTransitionPriority)
          .empty());

  auto set = effect_stack.AffectedProperties(KeyframeEffect::kDefaultPriority);
  ASSERT_EQ(3u, set.size());
  EXPECT_TRUE(set.Contains(PropertyHandle(GetCSSPropertyColor())));
  EXPECT_TRUE(set.Contains(PropertyHandle(GetCSSPropertyTop())));
  EXPECT_TRUE(set.Contains(PropertyHandle(GetCSSPropertyLeft())));
}

TEST_F(AnimationEffectStackTest, AffectedPropertiesTransitionPriority) {
  Element* body = GetDocument().body();
  body->SetInlineStyleProperty(CSSPropertyID::kTransition, "color 10s");
  body->SetInlineStyleProperty(CSSPropertyID::kColor, "red");
  UpdateAllLifecyclePhasesForTest();

  body->SetInlineStyleProperty(CSSPropertyID::kColor, "blue");
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(body->GetElementAnimations());
  const EffectStack& effect_stack =
      body->GetElementAnimations()->GetEffectStack();

  EXPECT_TRUE(effect_stack.AffectedProperties(KeyframeEffect::kDefaultPriority)
                  .empty());

  auto set =
      effect_stack.AffectedProperties(KeyframeEffect::kTransitionPriority);
  ASSERT_EQ(1u, set.size());
  EXPECT_TRUE(set.Contains(PropertyHandle(GetCSSPropertyColor())));
}

}  // namespace blink
