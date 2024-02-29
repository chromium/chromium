// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

using CompositedPaintStatus = ElementAnimations::CompositedPaintStatus;

class MockClipPathPaintImageGenerator : public ClipPathPaintImageGenerator {
 public:
  scoped_refptr<Image> Paint(float zoom,
                             const gfx::RectF& reference_box,
                             const gfx::SizeF& clip_area_size,
                             const Node& node) override {
    return ClipPathPaintDefinition::Paint(zoom, reference_box, clip_area_size,
                                          node, 0 /* use a dummy worklet id */);
  }
  gfx::RectF ClipAreaRect(const Node& node,
                          const gfx::RectF& reference_box,
                          float zoom) const override {
    return ClipPathPaintDefinition::ClipAreaRect(node, reference_box, zoom);
  }
  Animation* GetAnimationIfCompositable(const Element* element) override {
    return ClipPathPaintDefinition::GetAnimationIfCompositable(element);
  }
  void Shutdown() override {}
};

class ClipPathPaintDefinitionTest : public PageTestBase {
 public:
  ClipPathPaintDefinitionTest() = default;
  ~ClipPathPaintDefinitionTest() override = default;

 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    MockClipPathPaintImageGenerator* generator =
        MakeGarbageCollected<MockClipPathPaintImageGenerator>();
    GetFrame().SetClipPathPaintImageGeneratorForTesting(generator);
    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
  }
};

// Test the case where there is a clip-path animation with two simple
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, SimpleClipPathAnimationNotFallback) {
  ScopedCompositeClipPathAnimationForTest composite_clip_path_animation(true);
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

// Test the case where a 2nd composited clip path animation causes a fallback to
// the main thread. In this case, the paint properties should update to avoid
// any crashes or paint worklets existing beyond their validity.
TEST_F(ClipPathPaintDefinitionTest, FallbackOnNonCompositableSecondAnimation) {
  ScopedCompositeClipPathAnimationForTest composite_clip_path_animation(true);
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
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(lo->FirstFragment().PaintProperties()->ClipPathMask());

  // Further frames shouldn't cause more property updates than necessary.
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(lo->NeedsPaintPropertyUpdate());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(lo->FirstFragment().PaintProperties()->ClipPathMask());
}

TEST_F(ClipPathPaintDefinitionTest, ClipBoundingBoxEncompassesAnimation) {
  ScopedCompositeClipPathAnimationForTest composite_clip_path_animation(true);
  SetBodyInnerHTML(R"HTML(
    <div id ="target" style="position: fixed; width: 100px; height: 100px">
    </div>
  )HTML");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  CSSPropertyID property_id = CSSPropertyID::kClipPath;
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(property_id, "inset(20% 20%)",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(property_id, "inset(-100% -100%)",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

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

  gfx::RectF reference_box = gfx::RectF(0, 0, 100, 100);
  EXPECT_EQ(ClipPathPaintDefinition::ClipAreaRect(*element, reference_box, 1.f),
            gfx::RectF(-100, -100, 300, 300));
}

}  // namespace blink
