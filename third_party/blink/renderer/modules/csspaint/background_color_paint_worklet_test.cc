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

  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));
}

// 1. When there are multiple active background-color animations, fall back to
// main.
// 2. When there is only one background-color animation with composite mode
// being replace, then we don't fall back to main.
TEST_F(BackgroundColorPaintWorkletTest, FallbackToMainMultipleAnimations) {
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

  Element* element = GetElementById("target");
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation1 = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  Animation* animation2 = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  UpdateAllLifecyclePhasesForTest();
  animation1->play();
  animation2->play();

  // Two active background-color animations, fall back to main.
  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 2u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));

  // Cancel one of them, so we only have one active background-color animation,
  // in this case, we don't fall back.
  animation1->cancel();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_TRUE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets));
}

// Test that calling BackgroundColorPaintWorkletProxyClient::Paint won't crash
// when the animated property value is empty.
TEST_F(BackgroundColorPaintWorkletTest, ProxyClientPaintNoCrash) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  BackgroundColorPaintWorklet::ProxyClientPaintForTest();
}

}  // namespace blink
