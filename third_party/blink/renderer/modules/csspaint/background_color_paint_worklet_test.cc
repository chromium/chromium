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
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

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
  base::Optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
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
  base::Optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
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

  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  base::Optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
}

TEST_F(BackgroundColorPaintWorkletTest, MultipleAnimationsFallback) {
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

  // Two active background-color animations, fall back to main.
  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 2u);
  Vector<Color> animated_colors;
  Vector<double> offsets;
  base::Optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintWorklet::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
}

// Test that calling BackgroundColorPaintWorkletProxyClient::Paint won't crash
// when the animated property value is empty.
TEST_F(BackgroundColorPaintWorkletTest, ProxyClientPaintWithNoPropertyValue) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(255, 0, 0)};
  Vector<double> offsets = {0, 1};
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  BackgroundColorPaintWorklet::ProxyClientPaintForTest(animated_colors, offsets,
                                                       property_values);
}

// Test that BackgroundColorPaintWorkletProxyClient::Paint won't crash if the
// progress of the animation is a negative number.
TEST_F(BackgroundColorPaintWorkletTest, ProxyClientPaintWithNegativeProgress) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(255, 0, 0)};
  Vector<double> offsets = {0, 1};
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  CompositorPaintWorkletInput::PropertyKey property_key(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      CompositorElementId(1u));
  CompositorPaintWorkletInput::PropertyValue property_value(-0.0f);
  property_values.insert(std::make_pair(property_key, property_value));
  BackgroundColorPaintWorklet::ProxyClientPaintForTest(animated_colors, offsets,
                                                       property_values);
}

// Test that BackgroundColorPaintWorkletProxyClient::Paint won't crash if the
// progress of the animation is > 1.
TEST_F(BackgroundColorPaintWorkletTest,
       ProxyClientPaintWithLargerThanOneProgress) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(255, 0, 0)};
  Vector<double> offsets = {0, 1};
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  CompositorPaintWorkletInput::PropertyKey property_key(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      CompositorElementId(1u));
  float progress = 1 + std::numeric_limits<float>::epsilon();
  CompositorPaintWorkletInput::PropertyValue property_value(progress);
  property_values.insert(std::make_pair(property_key, property_value));
  BackgroundColorPaintWorklet::ProxyClientPaintForTest(animated_colors, offsets,
                                                       property_values);
}

// Test that BackgroundColorPaintWorkletProxyClient::Paint won't crash when the
// largest offset is not exactly one.
TEST_F(BackgroundColorPaintWorkletTest, ProxyClientPaintWithCloseToOneOffset) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(0, 255, 255),
                                   Color(255, 0, 0)};
  Vector<double> offsets = {0, 0.6, 0.99999};
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  CompositorPaintWorkletInput::PropertyKey property_key(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      CompositorElementId(1u));
  float progress = 1 - std::numeric_limits<float>::epsilon();
  CompositorPaintWorkletInput::PropertyValue property_value(progress);
  property_values.insert(std::make_pair(property_key, property_value));
  BackgroundColorPaintWorklet::ProxyClientPaintForTest(animated_colors, offsets,
                                                       property_values);
}

}  // namespace blink
