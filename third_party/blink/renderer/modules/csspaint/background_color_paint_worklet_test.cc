// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/background_color_paint_worklet.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/inert_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

using BackgroundColorPaintWorkletTest = PageTestBase;

// Test the case when there is no animation attached to the element.
TEST_F(BackgroundColorPaintWorkletTest, FallbackToMainNoAnimation) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");
  Element* element = GetElementById("target");
  EXPECT_FALSE(element->GetElementAnimations());
  Vector<Color> animated_colors;
  Vector<double> offsets;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));
}

// Test that when an element has other animations but no background color
// animation, then we fall back to the main thread. Also testing that calling
// BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams do not crash.
TEST_F(BackgroundColorPaintWorkletTest, NoBGColorAnimationFallback) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

  CSSPropertyID property_id = CSSPropertyID::kColor;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      property_id, "red", SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(
      property_id, "green", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  model->SetComposite(EffectModel::kCompositeAccumulate);

  Element* element = GetElementById("target");
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  UpdateAllLifecyclePhasesForTest();
  animation->play();

  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));
  EXPECT_TRUE(animated_colors.IsEmpty());
  EXPECT_TRUE(offsets.IsEmpty());
}

// Test the case where the composite mode is not replace.
TEST_F(BackgroundColorPaintWorkletTest, FallbackToMainCompositeAccumulate) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

  CSSPropertyID property_id = CSSPropertyID::kBackgroundColor;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      property_id, "red", SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(
      property_id, "green", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  model->SetComposite(EffectModel::kCompositeAccumulate);

  Element* element = GetElementById("target");
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  UpdateAllLifecyclePhasesForTest();
  animation->play();
  EXPECT_FALSE(animation->CanCompositeBGColorAnim());

  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));
  EXPECT_FALSE(animation->CanCompositeBGColorAnim());
}

// Test that when there are multiple bgcolor animations on an Element, we
// composite the animation with the highest compositing order.
TEST_F(BackgroundColorPaintWorkletTest, MultipleAnimationsNotFallback) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

  CSSPropertyID property_id = CSSPropertyID::kBackgroundColor;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      property_id, "red", SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(
      property_id, "green", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);
  auto* model1 = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  Element* element = GetElementById("target");
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation1 = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model1, timing), timeline,
      exception_state);

  start_keyframe->SetCSSPropertyValue(
      property_id, "blue", SecureContextMode::kInsecureContext, nullptr);
  end_keyframe->SetCSSPropertyValue(
      property_id, "yellow", SecureContextMode::kInsecureContext, nullptr);
  keyframes.clear();
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);
  auto* model2 = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  Animation* animation2 = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model2, timing), timeline,
      exception_state);
  UpdateAllLifecyclePhasesForTest();
  animation1->play();
  animation2->play();
  EXPECT_FALSE(animation1->CanCompositeBGColorAnim());
  EXPECT_FALSE(animation2->CanCompositeBGColorAnim());

  // Two active background-color animations, fall back to main.
  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 2u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  EXPECT_TRUE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));
  EXPECT_FALSE(animation1->CanCompositeBGColorAnim());
  EXPECT_TRUE(animation2->CanCompositeBGColorAnim());
  EXPECT_EQ(animated_colors.size(), 2u);
  // The animated_colors should be blue and yellow.
  EXPECT_EQ(animated_colors[0].Red(), 0);
  EXPECT_EQ(animated_colors[0].Green(), 0);
  EXPECT_EQ(animated_colors[0].Blue(), 255);
  EXPECT_EQ(animated_colors[1].Red(), 255);
  EXPECT_EQ(animated_colors[1].Green(), 255);
  EXPECT_EQ(animated_colors[1].Blue(), 0);
}

// Test that calling BackgroundColorPaintWorkletProxyClient::Paint won't crash
// when the animated property value is empty.
TEST_F(BackgroundColorPaintWorkletTest, ProxyClientPaintNoCrash) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  BackgroundColorPaintWorklet::ProxyClientPaintForTest();
}

}  // namespace blink
