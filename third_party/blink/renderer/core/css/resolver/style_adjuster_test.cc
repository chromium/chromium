// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleAdjusterTest : public RenderingTest {
 public:
  StyleAdjusterTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(StyleAdjusterTest, TouchActionPropagatedAcrossIframes) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; } iframe { display: block; } </style>
    <iframe id='owner' src='http://test.com' width='500' height='500'
    style='touch-action: none'>
    </iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin: 0; } #target { width: 200px; height: 200px; }
    </style>
    <div id='target' style='touch-action: pinch-zoom'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = ChildDocument().getElementById("target");
  EXPECT_EQ(TouchAction::kNone,
            target->GetComputedStyle()->GetEffectiveTouchAction());

  Element* owner = GetDocument().getElementById("owner");
  owner->setAttribute(html_names::kStyleAttr, "touch-action: auto");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPinchZoom,
            target->GetComputedStyle()->GetEffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionPanningReEnabledByScrollers) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>#ancestor { margin: 0; touch-action: pinch-zoom; }
    #scroller { overflow: scroll; width: 100px; height: 100px; }
    #target { width: 200px; height: 200px; } </style>
    <div id='ancestor'><div id='scroller'><div id='target'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(TouchAction::kManipulation,
            target->GetComputedStyle()->GetEffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionPropagatedWhenAncestorStyleChanges) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>#ancestor { margin: 0; touch-action: pan-x; }
    #potential-scroller { width: 100px; height: 100px; overflow: hidden; }
    #target { width: 200px; height: 200px; }</style>
    <div id='ancestor'><div id='potential-scroller'><div id='target'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(TouchAction::kPanX,
            target->GetComputedStyle()->GetEffectiveTouchAction());

  Element* ancestor = GetDocument().getElementById("ancestor");
  ancestor->setAttribute(html_names::kStyleAttr, "touch-action: pan-y");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPanY,
            target->GetComputedStyle()->GetEffectiveTouchAction());

  Element* potential_scroller =
      GetDocument().getElementById("potential-scroller");
  potential_scroller->setAttribute(html_names::kStyleAttr, "overflow: scroll");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPan,
            target->GetComputedStyle()->GetEffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, TouchActionRestrictedByLowerAncestor) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='ancestor' style='touch-action: pan'>
    <div id='parent' style='touch-action: pan-right pan-y'>
    <div id='target' style='touch-action: pan-x'>
    </div></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");
  EXPECT_EQ(TouchAction::kPanRight,
            target->GetComputedStyle()->GetEffectiveTouchAction());

  Element* parent = GetDocument().getElementById("parent");
  parent->setAttribute(html_names::kStyleAttr, "touch-action: auto");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(TouchAction::kPanX,
            target->GetComputedStyle()->GetEffectiveTouchAction());
}

TEST_F(StyleAdjusterTest, AdjustOverflow) {
  ScopedOverflowClipForTest overflow_clip_feature_enabler(true);
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id='clipauto' style='overflow-x: clip; overflow-y: auto;'>
    <div id='autoclip' style='overflow-x: auto; overflow-y: clip;'>
    <div id='clipclip' style='overflow-x: clip; overflow-y: clip;'>
    <div id='visclip' style='overflow-x: visible; overflow-y: clip;'>
    <div id='clipvis' style='overflow-x: clip; overflow-y: visible;'>
    <div id='hiddenvis' style='overflow-x: hidden; overflow-y: visible;'>
    <div id='vishidden' style='overflow-x: visible; overflow-y: hidden;'>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("clipauto");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kHidden, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kAuto, target->GetComputedStyle()->OverflowY());

  target = GetDocument().getElementById("autoclip");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kAuto, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kHidden, target->GetComputedStyle()->OverflowY());

  target = GetDocument().getElementById("clipclip");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kClip, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kClip, target->GetComputedStyle()->OverflowY());

  target = GetDocument().getElementById("visclip");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kVisible, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kClip, target->GetComputedStyle()->OverflowY());

  target = GetDocument().getElementById("clipvis");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kClip, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kVisible, target->GetComputedStyle()->OverflowY());

  target = GetDocument().getElementById("vishidden");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kAuto, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kHidden, target->GetComputedStyle()->OverflowY());

  target = GetDocument().getElementById("hiddenvis");
  ASSERT_TRUE(target);
  EXPECT_EQ(EOverflow::kHidden, target->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kAuto, target->GetComputedStyle()->OverflowY());
}

}  // namespace blink
