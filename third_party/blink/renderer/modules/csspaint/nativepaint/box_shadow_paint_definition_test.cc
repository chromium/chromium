// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/box_shadow_paint_definition.h"

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
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

namespace blink {

class BoxShadowPaintDefinitionTest : public PageTestBase {
 public:
  BoxShadowPaintDefinitionTest() = default;
  ~BoxShadowPaintDefinitionTest() override = default;
};

// Test the case where there is a box shadow animation with two simple
// keyframes and composite replace that will not fall back to main.
TEST_F(BoxShadowPaintDefinitionTest, SimpleBoxShadowAnimationNotFallback) {
  ScopedCompositeBoxShadowAnimationForTest composite_box_shadow_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kBoxShadow;
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

  // TODO(crbug.com/1258126): Add more checks for starting the animation, and
  // testing WorkletInputParameters
  EXPECT_TRUE(BoxShadowPaintDefinition::GetAnimationIfCompositable(element));
}

// Test the case when there is no animation attached to the element.
TEST_F(BoxShadowPaintDefinitionTest, FallbackToMainNoAnimation) {
  ScopedCompositeBoxShadowAnimationForTest composite_box_shadow_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");
  Element* element = GetElementById("target");
  EXPECT_FALSE(element->GetElementAnimations());
}

// Test that when an element has other animations but no box shadow
// animation, then we fall back to the main thread. Also testing that calling
// BoxShadowPaintDefinition::GetBoxShadowPaintWorkletParams do not crash.
TEST_F(BoxShadowPaintDefinitionTest, NoBoxShadowAnimationFallback) {
  ScopedCompositeBoxShadowAnimationForTest composite_box_shadow_animation(true);
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

  EXPECT_FALSE(BoxShadowPaintDefinition::GetAnimationIfCompositable(element));
}

// Test the case where the composite mode is not replace.
TEST_F(BoxShadowPaintDefinitionTest, FallbackToMainCompositeAccumulate) {
  ScopedCompositeBoxShadowAnimationForTest composite_box_shadow_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kBoxShadow;
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

  bool check = false;
  if (BoxShadowPaintDefinition::GetAnimationIfCompositable(element)) {
    check = true;
  }

  EXPECT_FALSE(check);
}

// Test the case where the element has multiple box shadow animations.
TEST_F(BoxShadowPaintDefinitionTest, MultipleAnimationsFallback) {
  ScopedCompositeBoxShadowAnimationForTest composite_box_shadow_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kBoxShadow;
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

  bool check = false;
  if (BoxShadowPaintDefinition::GetAnimationIfCompositable(element)) {
    check = true;
  }

  EXPECT_FALSE(check);
}

}  // namespace blink
