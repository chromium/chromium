// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_animation_curve.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/compositor_animation_color_curve.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/native_paint_worklet_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CompositorAnimationCurveTest : public RenderingTest {
 public:
  CompositorAnimationCurveTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  using ColorKeyframe = CompositorAnimationColorCurve::TypedKeyframe;

  // Each test is expected to create a single animation, though the animation
  // may affect multiple properties.
  Animation* GetAnimation(const char* id = "target") {
    UpdateAllLifecyclePhasesForTest();
    Element* target = GetElementById(id);
    ElementAnimations* element_animations = target->GetElementAnimations();
    EXPECT_TRUE(element_animations);
    EXPECT_EQ(element_animations->Animations().size(), 1u);
    return element_animations->Animations().begin()->key;
  }

  scoped_refptr<CompositorAnimationColorCurve> ExtractColorCurve(
      CSSPropertyID id,
      const char* target_id = "target") {
    Animation* animation = GetAnimation(target_id);
    return CompositorAnimationColorCurve::Create(animation,
                                                 CSSPropertyName(id));
  }
};

TEST_F(CompositorAnimationCurveTest, KeywordColors) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        from { background-color: black; }
        to { background-color: white; }
      }
      #target {
        height: 100px;
        width: 100x;
        animation: colorize 1s linear;
      }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_EQ(curve->Size(), 2u);
  EXPECT_FALSE(curve->HasStyleDependency());

  const ColorKeyframe& first = curve->GetTypedKeyframe(0);
  const ColorKeyframe& last = curve->GetTypedKeyframe(1);

  EXPECT_EQ(first.offset, 0);
  EXPECT_EQ(first.value, Color::kBlack);
  EXPECT_EQ(last.offset, 1);
  EXPECT_EQ(last.value, Color::kWhite);

  // If we force and update anyways, we get a new smart pointer to the same
  // underlying object.
  scoped_refptr<CompositorAnimationCurve> updated_curve =
      curve->UpdateKeyframeSnapshot(GetAnimation());
  EXPECT_EQ(curve.get(), updated_curve.get());
}

TEST_F(CompositorAnimationCurveTest, VariableSubstitedColors) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        from { background-color: var(--color1); }
        to { background-color: var(--color2); }
      }
      #target {
        --color1: red;
        --color2: green;
        height: 100px;
        width: 100x;
        animation: colorize 1s linear;
      }
      #target.updated {
        --color1: black;
        --color2: white;
      }
    }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_EQ(curve->Size(), 2u);
  EXPECT_TRUE(curve->HasStyleDependency());

  const ColorKeyframe& first = curve->GetTypedKeyframe(0);
  const ColorKeyframe& last = curve->GetTypedKeyframe(1);

  // Variable references are resolved.
  EXPECT_EQ(first.offset, 0);
  EXPECT_EQ(first.value, Color(255, 0, 0));
  EXPECT_EQ(last.offset, 1);
  EXPECT_EQ(last.value, Color(0, 128, 0));

  // None of the keyframe values were affected by the snapshot update.
  // both smart pointers refer to the same underlying  object.
  scoped_refptr<CompositorAnimationCurve> updated_curve =
      curve->UpdateKeyframeSnapshot(GetAnimation());
  EXPECT_EQ(curve.get(), updated_curve.get());

  GetElementById("target")->classList().add({"updated"}, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  // This time the snapshot update generates a new curve.
  updated_curve = curve->UpdateKeyframeSnapshot(GetAnimation());
  EXPECT_NE(curve.get(), updated_curve.get());
  EXPECT_EQ(curve->GetTypedKeyframe(0).value, Color(255, 0, 0));
  EXPECT_EQ(curve->GetTypedKeyframe(1).value, Color(0, 128, 0));
  CompositorAnimationColorCurve* color_curve =
      static_cast<CompositorAnimationColorCurve*>(updated_curve.get());
  EXPECT_EQ(color_curve->GetTypedKeyframe(0).value, Color::kBlack);
  EXPECT_EQ(color_curve->GetTypedKeyframe(1).value, Color::kWhite);
}

TEST_F(CompositorAnimationCurveTest, ColorCurveWithNeutralKeyframe) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        to { background-color: green; }
      }
      #target {
        height: 100px;
        width: 100x;
        background-color: red;
        animation: colorize 1s linear;
      }
    }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_TRUE(!curve);
}

TEST_F(CompositorAnimationCurveTest, StyleDependentColorMix) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root {
        --theme: red;
      }
      :root.updated {
        --theme: blue;
      }
      @keyframes colorize {
        from { background-color: color-mix(in srgb, var(--theme) 70%, white); }
        to   { background-color: color-mix(in srgb, var(--theme) 80%, black); }
      }
      #target {
        height: 100px;
        width: 100x;
        animation: colorize 1s linear;
      }
    }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_TRUE(curve->HasStyleDependency());
  EXPECT_EQ(curve->GetTypedKeyframe(0).value.Rgb(), 0xFFFF4D4D);

  GetDocument().documentElement()->classList().add({"updated"},
                                                   ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  curve = ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_TRUE(curve->HasStyleDependency());
  EXPECT_EQ(curve->GetTypedKeyframe(0).value.Rgb(), 0xFF4D4DFF);
}

TEST_F(CompositorAnimationCurveTest, SampleAndIntervalCacheForwardAndBackward) {
  const auto curve = CompositorAnimationColorCurve::CreateForTesting(
      {Color(255, 0, 0), Color(0, 255, 0), Color(0, 0, 255),
       Color(255, 255, 255)},
      {0.0, 0.25, 0.75, 1.0}, CSSPropertyName(CSSPropertyID::kBackgroundColor));
  EXPECT_EQ(curve->ComputeKeyframeIndex(0.1), 0u);
  EXPECT_EQ(curve->ComputeKeyframeIndex(0.5), 1u);
  EXPECT_EQ(curve->ComputeKeyframeIndex(0.9), 2u);
  EXPECT_EQ(curve->ComputeKeyframeIndex(0.5), 1u);
  EXPECT_EQ(curve->ComputeKeyframeIndex(0.1), 0u);

  SkColor4f sampled = SkColors::kTransparent;
  curve->Sample(0.25f, &sampled);
  EXPECT_EQ(sampled, SkColors::kGreen);
}

TEST_F(CompositorAnimationCurveTest,
       UpdateKeyframeSnapshotDuringSlotReassignment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        from { background-color: currentcolor; }
        to { background-color: var(--color2); }
      }
      #wrapper {
        color: rgb(10, 20, 30);
        --color2: rgb(0, 128, 0);
      }
      #wrapper.updated {
        color: rgb(40, 50, 60);
      }
      #target, #sibling {
        height: 100px;
        width: 100px;
        animation: colorize 1s linear;
      }
    </style>
    <div id="wrapper"></div>
  )HTML");

  Element* wrapper = GetElementById("wrapper");
  ASSERT_TRUE(wrapper);
  ShadowRoot& shadow_root =
      wrapper->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<slot name='a'></slot>");
  wrapper->SetInnerHTMLWithoutTrustedTypes(
      "<div id='target' slot='a'></div><div id='sibling' slot='a'></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  Element* sibling = GetElementById("sibling");
  ASSERT_TRUE(target);
  ASSERT_TRUE(sibling);

  Animation* target_animation = GetAnimation("target");
  ASSERT_TRUE(target_animation);
  scoped_refptr<CompositorAnimationColorCurve> target_curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor, "target");
  ASSERT_TRUE(target_curve);
  EXPECT_TRUE(target_curve->HasStyleDependency());
  EXPECT_EQ(target_curve->GetTypedKeyframe(0).value, Color(10, 20, 30));

  NativePaintWorkletData* npw_data =
      target->EnsureElementAnimations().EnsureBackgroundColorNpwData(target);
  ASSERT_TRUE(npw_data);
  npw_data->SetStatus(
      NativePaintWorkletData::CompositedPaintStatus::kComposited);
  npw_data->SetAnimation(target_animation);
  npw_data->SetAnimationCurve(std::move(target_curve));
  npw_data->SetNeedsKeyframeSnapshot();

  // Reassigning the slot marks slot assignment dirty without immediately
  // detaching `target`'s layout tree. Resolving `currentcolor` inside
  // `GetAnimationCurve()` -> `UpdateKeyframeSnapshot()` triggers slot
  // assignment recalculation, detaching `target` and canceling its animation.
  target->setAttribute(html_names::kSlotAttr, AtomicString("b"));
  ASSERT_TRUE(target->GetComputedStyle());

  scoped_refptr<CompositorAnimationCurve> result_curve =
      npw_data->GetAnimationCurve();
  EXPECT_FALSE(result_curve);
  EXPECT_FALSE(target->GetComputedStyle());
  EXPECT_EQ(npw_data->GetCompositedPaintStatus(),
            NativePaintWorkletData::CompositedPaintStatus::kNoAnimation);
  EXPECT_FALSE(npw_data->NeedsKeyframeSnapshotUpdate());

  // Verify that the remaining slotted element continues to update its
  // style-dependent keyframe snapshot normally.
  scoped_refptr<CompositorAnimationColorCurve> sibling_curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor, "sibling");
  ASSERT_TRUE(sibling_curve);
  EXPECT_EQ(sibling_curve->GetTypedKeyframe(0).value, Color(10, 20, 30));

  wrapper->classList().add({"updated"}, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Animation* sibling_animation = GetAnimation("sibling");
  ASSERT_TRUE(sibling_animation);
  scoped_refptr<CompositorAnimationCurve> updated_sibling_curve =
      sibling_curve->UpdateKeyframeSnapshot(sibling_animation);
  ASSERT_TRUE(updated_sibling_curve);
  EXPECT_NE(sibling_curve.get(), updated_sibling_curve.get());
  auto* updated_color_curve =
      static_cast<CompositorAnimationColorCurve*>(updated_sibling_curve.get());
  EXPECT_EQ(updated_color_curve->GetTypedKeyframe(0).value, Color(40, 50, 60));

  // Direct `UpdateKeyframeSnapshot` when a slot reassignment detaches the
  // element mid-resolution should also return nullptr rather than a stale
  // curve, and `Create` on the detached element should return nullptr.
  sibling->setAttribute(html_names::kSlotAttr, AtomicString("b"));
  ASSERT_TRUE(sibling->GetComputedStyle());
  EXPECT_FALSE(
      updated_sibling_curve->UpdateKeyframeSnapshot(sibling_animation));
  EXPECT_FALSE(sibling->GetComputedStyle());
  EXPECT_FALSE(CompositorAnimationColorCurve::Create(
      sibling_animation, CSSPropertyName(CSSPropertyID::kBackgroundColor)));
}

TEST_F(CompositorAnimationCurveTest,
       CrossDocumentSlotReassignmentAndTimelineLifecycle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        from { background-color: currentcolor; }
        to { background-color: rgb(200, 0, 0); }
      }
      #target {
        width: 50px;
        height: 50px;
        color: rgb(10, 20, 30);
        animation: colorize 100s linear;
      }
    </style>
    <iframe></iframe>
    <div id="target"></div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      @keyframes clippy {
        from { clip-path: inset(0px); }
        to { clip-path: inset(10px); }
      }
      #item2.animate {
        animation: clippy 100s linear;
      }
      div {
        width: 20px;
        height: 10px;
      }
    </style>
    <div id="wrapper">
      <div id="item1" slot="a"></div>
      <div id="item2" slot="a"></div>
    </div>
  )HTML");

  Element* wrapper = ChildDocument().getElementById(AtomicString("wrapper"));
  ASSERT_TRUE(wrapper);
  ShadowRoot& shadow_root =
      wrapper->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<slot name='a'></slot>");
  Element* item1 = ChildDocument().getElementById(AtomicString("item1"));
  Element* item2 = ChildDocument().getElementById(AtomicString("item2"));
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  item2->classList().add({"animate"}, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  Animation* parent_anim = GetAnimation();
  ASSERT_TRUE(parent_anim);

  ElementAnimations* item2_animations = item2->GetElementAnimations();
  ASSERT_TRUE(item2_animations);
  ASSERT_EQ(item2_animations->Animations().size(), 1u);
  Animation* child_anim = item2_animations->Animations().begin()->key;
  ASSERT_TRUE(child_anim);

  // If a same-document animation is already marked outdated and then retargeted
  // to an element in another document, SetOutdated() must synchronously update
  // timing so it does not remain outdated into style/paint.
  child_anim->SetOutdated();
  EXPECT_TRUE(child_anim->Outdated());
  To<KeyframeEffect>(child_anim->effect())->setTarget(target);
  EXPECT_FALSE(child_anim->Outdated());

  // Attach the parent animation to the child document's timeline and retarget
  // it to the child document while reassigning `item2`'s slot.
  parent_anim->setTimeline(&ChildDocument().Timeline());
  To<KeyframeEffect>(parent_anim->effect())->setTarget(item1);
  item2->setAttribute(html_names::kSlotAttr, AtomicString("b"));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPaintClean);
}

}  // end namespace blink
