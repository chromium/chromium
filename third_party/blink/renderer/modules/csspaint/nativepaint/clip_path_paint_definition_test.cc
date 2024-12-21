// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using CompositedPaintStatus = ElementAnimations::CompositedPaintStatus;

class ClipPathPaintDefinitionTest : public PageTestBase {
 public:
  ClipPathPaintDefinitionTest() = default;
  ~ClipPathPaintDefinitionTest() override = default;

 protected:
  void SetUp() override {
    scoped_composite_clip_path_animation =
        std::make_unique<ScopedCompositeClipPathAnimationForTest>(true);
    scoped_composite_bgcolor_animation =
        std::make_unique<ScopedCompositeBGColorAnimationForTest>(false);
    PageTestBase::SetUp();
    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
  }

 private:
  std::unique_ptr<ScopedCompositeClipPathAnimationForTest>
      scoped_composite_clip_path_animation;
  std::unique_ptr<ScopedCompositeBGColorAnimationForTest>
      scoped_composite_bgcolor_animation;
};

// Test the case where there is a clip-path animation with two simple
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, SimpleClipPathAnimationNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kClipPath;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(property_id, "circle(50% at 50% 50%)",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(property_id, "circle(30% at 30% 30%)",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  model->SetComposite(EffectModel::kCompositeReplace);

  Element* element = GetElementById("target");
  LayoutObject* lo = element->GetLayoutObject();
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  animation->play();

  UpdateAllLifecyclePhasesForTest();

  // Ensure that the paint property was set correctly - composited animation
  // uses a mask based clip.
  EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kComposited);
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  EXPECT_EQ(ClipPathPaintDefinition::GetAnimationIfCompositable(element),
            animation);
}

TEST_F(ClipPathPaintDefinitionTest, ClipPathAnimationCancel) {
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kClipPath;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(property_id, "circle(50% at 50% 50%)",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(property_id, "circle(30% at 30% 30%)",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  model->SetComposite(EffectModel::kCompositeReplace);

  Element* element = GetElementById("target");
  LayoutObject* lo = element->GetLayoutObject();
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  animation->play();

  UpdateAllLifecyclePhasesForTest();

  animation->cancel();
  // Cancelling the animation should trigger a repaint to clear the composited
  // paint image.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(lo->NeedsPaintPropertyUpdate());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kNoAnimation);
  UpdateAllLifecyclePhasesForTest();

  // Further frames shouldn't cause more property updates.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(lo->NeedsPaintPropertyUpdate());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kNoAnimation);
}

// Test the case where a 2nd composited clip path animation causes a fallback to
// the main thread. In this case, the paint properties should update to avoid
// any crashes or paint worklets existing beyond their validity.
TEST_F(ClipPathPaintDefinitionTest, FallbackOnNonCompositableSecondAnimation) {
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kClipPath;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(property_id, "circle(50% at 50% 50%)",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(property_id, "circle(30% at 30% 30%)",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
  model->SetComposite(EffectModel::kCompositeReplace);

  Element* element = GetElementById("target");
  LayoutObject* lo = element->GetLayoutObject();
  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());
  Animation* animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
      exception_state);
  animation->play();

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(lo->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(lo->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();

  // After adding a single animation, all should be well.
  EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
  EXPECT_TRUE(element->GetElementAnimations());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kComposited);
  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 1u);
  EXPECT_EQ(ClipPathPaintDefinition::GetAnimationIfCompositable(element),
            animation);

  Timing timing2;
  timing2.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  timing2.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(5));

  Animation* animation2 = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing2), timeline,
      exception_state);
  animation2->play();

  EXPECT_EQ(element->GetElementAnimations()->Animations().size(), 2u);
  // If support for delayed animations is added, this check will fail. This test
  // should be updated to create a non compositible animation through other
  // means in this case.
  EXPECT_EQ(ClipPathPaintDefinition::GetAnimationIfCompositable(element),
            nullptr);

  // After adding a second animation with a delay, we gracefully fallback.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(lo->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(lo->ShouldDoFullPaintInvalidation());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kNeedsRepaint);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(lo->FirstFragment().PaintProperties()->ClipPathMask());

  // Further frames shouldn't cause more property updates than necessary.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(lo->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(lo->NeedsPaintPropertyUpdate());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kNotComposited);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(lo->FirstFragment().PaintProperties()->ClipPathMask());
}

TEST_F(ClipPathPaintDefinitionTest,
       NoInvalidationsOnPseudoWithTransformAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target:after{
        content:"";
      }
    </style>
    <span id="target"></span>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id_cp = CSSPropertyID::kClipPath;
  Persistent<StringKeyframe> start_keyframe_cp =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe_cp->SetCSSPropertyValue(
      property_id_cp, "circle(50% at 50% 50%)",
      SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> mid_keyframe_cp =
      MakeGarbageCollected<StringKeyframe>();
  mid_keyframe_cp->SetCSSPropertyValue(property_id_cp, "circle(50% at 50% 50%)",
                                       SecureContextMode::kInsecureContext,
                                       nullptr);
  Persistent<StringKeyframe> end_keyframe_cp =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe_cp->SetCSSPropertyValue(property_id_cp, "circle(30% at 30% 30%)",
                                       SecureContextMode::kInsecureContext,
                                       nullptr);

  StringKeyframeVector keyframes_cp;
  keyframes_cp.push_back(start_keyframe_cp);
  keyframes_cp.push_back(mid_keyframe_cp);
  keyframes_cp.push_back(end_keyframe_cp);

  auto* model_cp =
      MakeGarbageCollected<StringKeyframeEffectModel>(keyframes_cp);
  model_cp->SetComposite(EffectModel::kCompositeReplace);

  CSSPropertyID property_id_tf = CSSPropertyID::kTransform;
  Persistent<StringKeyframe> start_keyframe_tf =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe_tf->SetCSSPropertyValue(property_id_tf, "rotate(10deg)",
                                         SecureContextMode::kInsecureContext,
                                         nullptr);
  Persistent<StringKeyframe> end_keyframe_tf =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe_tf->SetCSSPropertyValue(property_id_tf, "rotate(360deg)",
                                       SecureContextMode::kInsecureContext,
                                       nullptr);

  StringKeyframeVector keyframes_tf;
  keyframes_tf.push_back(start_keyframe_tf);
  keyframes_tf.push_back(end_keyframe_tf);

  auto* model_tf =
      MakeGarbageCollected<StringKeyframeEffectModel>(keyframes_tf);
  model_tf->SetComposite(EffectModel::kCompositeReplace);

  Element* element_main = GetElementById("target");
  Element* element_pseudo = To<Element>(element_main->PseudoAwareFirstChild());

  NonThrowableExceptionState exception_state;
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(&GetDocument());

  Animation* animation_tf = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element_main, model_tf, timing),
      timeline, exception_state);
  animation_tf->play();

  Animation* animation_cp = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element_pseudo, model_cp, timing),
      timeline, exception_state);
  animation_cp->play();

  UpdateAllLifecyclePhasesForTest();

  // Set up animations for ticking.

  GetDocument().Timeline().ResetForTesting();
  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation_tf->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  // Check for correct state on the first frame. In all cases this should be
  // correct (basic behavior for the feature)

  LayoutObject* lo = element_pseudo->GetLayoutObject();
  EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
  EXPECT_TRUE(element_pseudo->GetElementAnimations());
  EXPECT_EQ(element_pseudo->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kComposited);

  // Run lifecycle again, and advance the animation time so that style
  // invalidation occurs. In this case, the correct behavior is that the
  // animation should be in steady-state. The transform animation on the root
  // element should NOT cause invalidations for the pseudo.

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(250));
  animation_tf->Update(kTimingUpdateForAnimationFrame);
  GetDocument().GetPendingAnimations().Update(
      GetDocument().GetFrame()->View()->GetPaintArtifactCompositor(), false);

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(lo->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(lo->NeedsPaintPropertyUpdate());
  EXPECT_EQ(element_pseudo->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kComposited);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
}

}  // namespace blink
