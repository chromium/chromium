// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_definition.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/inert_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

namespace blink {

class BackgroundColorPaintDefinitionTest : public PageTestBase {
 public:
  BackgroundColorPaintDefinitionTest() = default;
  ~BackgroundColorPaintDefinitionTest() override = default;

  void RunPaintForTest(const Vector<Color>& animated_colors,
                       const Vector<double>& offsets,
                       const CompositorPaintWorkletJob::AnimatedPropertyValues&
                           property_values) {
    BackgroundColorPaintDefinition definition;
    definition.PaintForTest(animated_colors, offsets, property_values);
  }
};

// Test the case where there is a background-color animation with two simple
// keyframes that will not fall back to main.
TEST_F(BackgroundColorPaintDefinitionTest, SimpleBGColorAnimationNotFallback) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

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
  model->SetComposite(EffectModel::kCompositeReplace);

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
  absl::optional<double> progress;
  EXPECT_TRUE(BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
}

// Test the case when there is no animation attached to the element.
TEST_F(BackgroundColorPaintDefinitionTest, FallbackToMainNoAnimation) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");
  Element* element = GetElementById("target");
  EXPECT_FALSE(element->GetElementAnimations());
  Vector<Color> animated_colors;
  Vector<double> offsets;
  absl::optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
}

// Test that when an element has other animations but no background color
// animation, then we fall back to the main thread. Also testing that calling
// BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams do not crash.
TEST_F(BackgroundColorPaintDefinitionTest, NoBGColorAnimationFallback) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

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
  absl::optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
  EXPECT_TRUE(animated_colors.IsEmpty());
  EXPECT_TRUE(offsets.IsEmpty());
}

// Test the case where the composite mode is not replace.
TEST_F(BackgroundColorPaintDefinitionTest, FallbackToMainCompositeAccumulate) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

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
  absl::optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
}

TEST_F(BackgroundColorPaintDefinitionTest, MultipleAnimationsFallback) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

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
  absl::optional<double> progress;
  EXPECT_FALSE(BackgroundColorPaintDefinition::GetBGColorPaintWorkletParams(
      element, &animated_colors, &offsets, &progress));
}

// Test that style->CompositablePaintAnimationChanged() should be true in the
// case where we initially have one background-color animation, and then changed
// to have two background-color animation on the element.
TEST_F(BackgroundColorPaintDefinitionTest,
       TriggerRepaintCompositedToNonComposited) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

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
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().ResolveStyle(element,
                                                    StyleRecalcContext());
  EXPECT_FALSE(style->HasCurrentBackgroundColorAnimation());

  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation1 = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model1, timing), timeline,
      exception_state);
  animation1->play();
  ASSERT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  style = GetDocument().GetStyleResolver().ResolveStyle(element,
                                                        StyleRecalcContext());
  // Previously no background-color animation, now it has. This should trigger
  // a repaint, see ComputedStyle::UpdatePropertySpecificDifferences().
  EXPECT_TRUE(style->HasCurrentBackgroundColorAnimation());
  style->ResetHasCurrentBackgroundColorAnimation();
  style->ResetCompositablePaintAnimationChanged();

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
  animation1->play();
  animation2->play();

  ASSERT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 2u);
  style = GetDocument().GetStyleResolver().ResolveStyle(element,
                                                        StyleRecalcContext());
  EXPECT_TRUE(style->HasCurrentBackgroundColorAnimation());
  // CompositablePaintAnimationChanged() being true will trigger a repaint. See
  // ComputedStyle::UpdatePropertySpecificDifferences().
  EXPECT_TRUE(style->CompositablePaintAnimationChanged());
}

// Test that style->CompositablePaintAnimationChanged() should be true in the
// case where we initially have one background-color animation, and then we
// changed one of the animation's keyframes.
TEST_F(BackgroundColorPaintDefinitionTest, TriggerRepaintChangedKeyframe) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

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
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().ResolveStyle(element,
                                                    StyleRecalcContext());
  EXPECT_FALSE(style->HasCurrentBackgroundColorAnimation());

  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  animation->play();
  ASSERT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  style = GetDocument().GetStyleResolver().ResolveStyle(element,
                                                        StyleRecalcContext());
  // Previously no background-color animation, now it has. This should trigger
  // a repaint, see ComputedStyle::UpdatePropertySpecificDifferences().
  EXPECT_TRUE(style->HasCurrentBackgroundColorAnimation());
  style->ResetHasCurrentBackgroundColorAnimation();
  style->ResetCompositablePaintAnimationChanged();

  start_keyframe->SetCSSPropertyValue(
      property_id, "red", SecureContextMode::kInsecureContext, nullptr);
  end_keyframe->SetCSSPropertyValue(
      property_id, "yellow", SecureContextMode::kInsecureContext, nullptr);
  keyframes.clear();
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);
  animation->play();

  ASSERT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  style = GetDocument().GetStyleResolver().ResolveStyle(element,
                                                        StyleRecalcContext());
  EXPECT_TRUE(style->HasCurrentBackgroundColorAnimation());
  // CompositablePaintAnimationChanged() being true will trigger a repaint. See
  // ComputedStyle::UpdatePropertySpecificDifferences().
  EXPECT_TRUE(style->CompositablePaintAnimationChanged());
}

TEST_F(BackgroundColorPaintDefinitionTest,
       CompositablePaintAnimationChangedFlagInvalidatesPaint) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  SetBodyInnerHTML("<div id=target></div>");

  Element* element = GetElementById("target");
  ASSERT_TRUE(element);

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kBackgroundColor;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      property_id, "green", SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(
      property_id, "green", SecureContextMode::kInsecureContext, nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);
  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  auto* effect = MakeGarbageCollected<KeyframeEffect>(element, model, timing);

  auto* animation =
      Animation::Create(effect, &GetDocument().Timeline(), ASSERT_NO_EXCEPTION);

  animation->play();

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->ComputedStyleRef().HasCurrentBackgroundColorAnimation());
  EXPECT_TRUE(element->ComputedStyleRef().CompositablePaintAnimationChanged());

  // Unrelated style change to clear the CompositablePaintAnimationChanged
  // flag.
  element->SetInlineStyleProperty(CSSPropertyID::kWidth, "200px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(element->ComputedStyleRef().HasCurrentBackgroundColorAnimation());
  EXPECT_FALSE(element->ComputedStyleRef().CompositablePaintAnimationChanged());

  // Set compositor pending.
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0.5),
                          ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(animation->CompositorPending());
  element->SetNeedsAnimationStyleRecalc();
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(element->ComputedStyleRef().HasCurrentBackgroundColorAnimation());
  EXPECT_TRUE(element->ComputedStyleRef().CompositablePaintAnimationChanged());
  ASSERT_TRUE(element->GetLayoutObject());
  EXPECT_TRUE(element->GetLayoutObject()->ShouldCheckForPaintInvalidation());

  // Run paint.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(animation->CompositorPending());
  EXPECT_FALSE(element->GetLayoutObject()->ShouldCheckForPaintInvalidation());

  // Set compositor pending again. This time the current style already has
  // CompositablePaintAnimationChanged, hence the old and new styles are
  // identical, but the new style should still invalidate paint.
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0.7),
                          ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(animation->CompositorPending());
  element->SetNeedsAnimationStyleRecalc();
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(element->ComputedStyleRef().HasCurrentBackgroundColorAnimation());
  EXPECT_TRUE(element->ComputedStyleRef().CompositablePaintAnimationChanged());
  ASSERT_TRUE(element->GetLayoutObject());
  EXPECT_TRUE(element->GetLayoutObject()->ShouldCheckForPaintInvalidation());
}

// Test that calling BackgroundColorPaintWorkletProxyClient::Paint won't crash
// when the animated property value is empty.
TEST_F(BackgroundColorPaintDefinitionTest,
       ProxyClientPaintWithNoPropertyValue) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(255, 0, 0)};
  Vector<double> offsets = {0, 1};
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  RunPaintForTest(animated_colors, offsets, property_values);
}

// Test that BackgroundColorPaintWorkletProxyClient::Paint won't crash if the
// progress of the animation is a negative number.
TEST_F(BackgroundColorPaintDefinitionTest,
       ProxyClientPaintWithNegativeProgress) {
  ScopedCompositeBGColorAnimationForTest composite_bgcolor_animation(true);
  Vector<Color> animated_colors = {Color(0, 255, 0), Color(255, 0, 0)};
  Vector<double> offsets = {0, 1};
  CompositorPaintWorkletJob::AnimatedPropertyValues property_values;
  CompositorPaintWorkletInput::PropertyKey property_key(
      CompositorPaintWorkletInput::NativePropertyType::kBackgroundColor,
      CompositorElementId(1u));
  CompositorPaintWorkletInput::PropertyValue property_value(-0.0f);
  property_values.insert(std::make_pair(property_key, property_value));
  RunPaintForTest(animated_colors, offsets, property_values);
}

// Test that BackgroundColorPaintWorkletProxyClient::Paint won't crash if the
// progress of the animation is > 1.
TEST_F(BackgroundColorPaintDefinitionTest,
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
  RunPaintForTest(animated_colors, offsets, property_values);
}

// Test that BackgroundColorPaintWorkletProxyClient::Paint won't crash when the
// largest offset is not exactly one.
TEST_F(BackgroundColorPaintDefinitionTest,
       ProxyClientPaintWithCloseToOneOffset) {
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
  RunPaintForTest(animated_colors, offsets, property_values);
}

}  // namespace blink
