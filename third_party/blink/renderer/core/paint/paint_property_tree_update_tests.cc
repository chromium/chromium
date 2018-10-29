// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder_test.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

// Tests covering incremental updates of paint property trees.
class PaintPropertyTreeUpdateTest : public PaintPropertyTreeBuilderTest {};

INSTANTIATE_PAINT_TEST_CASE_P(PaintPropertyTreeUpdateTest);

TEST_P(PaintPropertyTreeUpdateTest,
       ThreadedScrollingDisabledMainThreadScrollReason) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #overflowA {
        position: absolute;
        overflow: scroll;
        width: 20px;
        height: 20px;
      }
      .forceScroll {
        height: 4000px;
      }
    </style>
    <div id='overflowA'>
      <div class='forceScroll'></div>
    </div>
    <div class='forceScroll'></div>
  )HTML");
  Element* overflow_a = GetDocument().getElementById("overflowA");
  EXPECT_FALSE(DocScroll()->ThreadedScrollingDisabled());
  EXPECT_FALSE(overflow_a->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->ThreadedScrollingDisabled());

  GetDocument().GetSettings()->SetThreadedScrollingEnabled(false);
  // TODO(pdr): The main thread scrolling setting should invalidate properties.
  GetDocument().View()->SetNeedsPaintPropertyUpdate();
  overflow_a->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(DocScroll()->ThreadedScrollingDisabled());
  EXPECT_TRUE(overflow_a->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->ThreadedScrollingDisabled());
}

TEST_P(PaintPropertyTreeUpdateTest,
       BackgroundAttachmentFixedMainThreadScrollReasonsWithNestedScrollers) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #overflowA {
        position: absolute;
        overflow: scroll;
        width: 20px;
        height: 20px;
      }
      #overflowB {
        position: absolute;
        overflow: scroll;
        width: 5px;
        height: 3px;
      }
      .backgroundAttachmentFixed {
        background-image: url('foo');
        background-attachment: fixed;
      }
      .forceScroll {
        height: 4000px;
      }
    </style>
    <div id='overflowA'>
      <div id='overflowB' class='backgroundAttachmentFixed'>
        <div class='forceScroll'></div>
      </div>
      <div class='forceScroll'></div>
    </div>
    <div class='forceScroll'></div>
  )HTML");
  Element* overflow_a = GetDocument().getElementById("overflowA");
  Element* overflow_b = GetDocument().getElementById("overflowB");

  EXPECT_TRUE(DocScroll()->HasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflow_a->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflow_b->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  overflow_b->removeAttribute("class");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(DocScroll()->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflow_a->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflow_b->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->HasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  overflow_b->setAttribute(HTMLNames::classAttr, "backgroundAttachmentFixed");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(DocScroll()->HasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflow_a->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(overflow_b->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest, ParentFrameMainThreadScrollReasons) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .fixedBackground {
        background-image: url('foo');
        background-attachment: fixed;
      }
    </style>
    <iframe></iframe>
    <div id='fixedBackground' class='fixedBackground'></div>
    <div id='forceScroll' style='height: 8888px;'></div>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='forceScroll' style='height: 8888px;'></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Document* parent = &GetDocument();
  EXPECT_TRUE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  Document* child = &ChildDocument();
  EXPECT_TRUE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  auto* fixed_background = GetDocument().getElementById("fixedBackground");
  fixed_background->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  fixed_background->setAttribute(HTMLNames::classAttr, "fixedBackground");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest, ChildFrameMainThreadScrollReasons) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <iframe></iframe>
    <div id='forceScroll' style='height: 8888px;'></div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 0; }
      .fixedBackground {
        background-image: url('foo');
        background-attachment: fixed;
      }
    </style>
    <div id='fixedBackground' class='fixedBackground'></div>
    <div id='forceScroll' style='height: 8888px;'></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Document* parent = &GetDocument();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  Document* child = &ChildDocument();
  EXPECT_TRUE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  auto* fixed_background = ChildDocument().getElementById("fixedBackground");
  fixed_background->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  fixed_background->setAttribute(HTMLNames::classAttr, "fixedBackground");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  EXPECT_TRUE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest,
       BackgroundAttachmentFixedMainThreadScrollReasonsWithFixedScroller) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #overflowA {
        position: absolute;
        overflow: scroll;
        width: 20px;
        height: 20px;
      }
      #overflowB {
        position: fixed;
        overflow: scroll;
        width: 5px;
        height: 3px;
      }
      .backgroundAttachmentFixed {
        background-image: url('foo');
        background-attachment: fixed;
      }
      .forceScroll {
        height: 4000px;
      }
    </style>
    <div id='overflowA'>
      <div id='overflowB' class='backgroundAttachmentFixed'>
        <div class='forceScroll'></div>
      </div>
      <div class='forceScroll'></div>
    </div>
    <div class='forceScroll'></div>
  )HTML");
  Element* overflow_a = GetDocument().getElementById("overflowA");
  Element* overflow_b = GetDocument().getElementById("overflowB");
  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();

  // This should be false. We are not as strict about main thread scrolling
  // reasons as we could be.
  EXPECT_TRUE(overflow_a->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflow_b->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_EQ(visual_viewport.GetScrollNode(), overflow_b->GetLayoutObject()
                                                 ->FirstFragment()
                                                 .PaintProperties()
                                                 ->ScrollTranslation()
                                                 ->ScrollNode()
                                                 ->Parent());

  // Removing a main thread scrolling reason should update the entire tree.
  overflow_b->removeAttribute("class");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(overflow_a->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflow_b->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(overflow_b->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ScrollTranslation()
                   ->ScrollNode()
                   ->Parent()
                   ->HasBackgroundAttachmentFixedDescendants());
}

TEST_P(PaintPropertyTreeUpdateTest, DescendantNeedsUpdateAcrossFrames) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='divWithTransform' style='transform: translate3d(1px,2px,3px);'>
      <iframe style='border: 7px solid black'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; }</style><div id='transform' style='transform: "
      "translate3d(4px, 5px, 6px); width: 100px; height: 200px'></div>");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhases();

  LayoutObject* div_with_transform =
      GetLayoutObjectByElementId("divWithTransform");
  LayoutObject* child_layout_view = ChildDocument().GetLayoutView();
  LayoutObject* inner_div_with_transform =
      ChildDocument().getElementById("transform")->GetLayoutObject();

  // Initially, no objects should need a descendant update.
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(div_with_transform->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(child_layout_view->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(inner_div_with_transform->DescendantNeedsPaintPropertyUpdate());

  // Marking the child div as needing a paint property update should propagate
  // up the tree and across frames.
  inner_div_with_transform->SetNeedsPaintPropertyUpdate();
  EXPECT_TRUE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(div_with_transform->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(child_layout_view->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(inner_div_with_transform->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(inner_div_with_transform->DescendantNeedsPaintPropertyUpdate());

  // After a lifecycle update, no nodes should need a descendant update.
  frame_view->UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(div_with_transform->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(child_layout_view->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(inner_div_with_transform->DescendantNeedsPaintPropertyUpdate());

  // A child frame marked as needing a paint property update should not be
  // skipped if the owning layout tree does not need an update.
  LocalFrameView* child_frame_view = ChildDocument().View();
  child_frame_view->SetNeedsPaintPropertyUpdate();
  EXPECT_TRUE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  frame_view->UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      child_frame_view->GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(child_frame_view->GetLayoutView()->NeedsPaintPropertyUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, UpdatingFrameViewContentClip) {
  SetBodyInnerHTML("hello world.");
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), DocContentClip()->ClipRect());
  GetDocument().View()->Resize(800, 599);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 599), DocContentClip()->ClipRect());
  GetDocument().View()->Resize(800, 600);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), DocContentClip()->ClipRect());
  GetDocument().View()->Resize(5, 5);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRoundedRect(0, 0, 5, 5), DocContentClip()->ClipRect());
}

// There is also FrameThrottlingTest.UpdatePaintPropertiesOnUnthrottling
// testing with real frame viewport intersection observer. This one tests
// paint property update with or without AllowThrottlingScope.
TEST_P(PaintPropertyTreeUpdateTest, BuildingStopsAtThrottledFrames) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='transform' style='transform: translate3d(4px, 5px, 6px);'>
    </div>
    <iframe id='iframe' sandbox></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='iframeTransform'
      style='transform: translate3d(4px, 5px, 6px);'/>
  )HTML");

  // Move the child frame offscreen so it becomes available for throttling.
  auto* iframe = ToHTMLIFrameElement(GetDocument().getElementById("iframe"));
  iframe->setAttribute(HTMLNames::styleAttr, "transform: translateY(5555px)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Ensure intersection observer notifications get delivered.
  test::RunPendingTasks();
  EXPECT_FALSE(GetDocument().View()->IsHiddenForThrottling());
  EXPECT_TRUE(ChildDocument().View()->IsHiddenForThrottling());

  auto* transform = GetLayoutObjectByElementId("transform");
  auto* iframe_layout_view = ChildDocument().GetLayoutView();
  auto* iframe_transform =
      ChildDocument().getElementById("iframeTransform")->GetLayoutObject();

  // Invalidate properties in the iframe and ensure ancestors are marked.
  iframe_transform->SetNeedsPaintPropertyUpdate();
  EXPECT_FALSE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframe_layout_view->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(iframe_layout_view->DescendantNeedsPaintPropertyUpdate());
  EXPECT_TRUE(iframe_transform->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframe_transform->DescendantNeedsPaintPropertyUpdate());

  transform->SetNeedsPaintPropertyUpdate();
  EXPECT_TRUE(transform->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->DescendantNeedsPaintPropertyUpdate());

  {
    DocumentLifecycle::AllowThrottlingScope throttling_scope(
        GetDocument().Lifecycle());
    EXPECT_FALSE(GetDocument().View()->ShouldThrottleRendering());
    EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRendering());

    // A lifecycle update should update all properties except those with
    // actively throttled descendants.
    GetDocument().View()->UpdateAllLifecyclePhases();
    EXPECT_FALSE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
    EXPECT_FALSE(
        GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
    EXPECT_FALSE(transform->NeedsPaintPropertyUpdate());
    EXPECT_FALSE(transform->DescendantNeedsPaintPropertyUpdate());
    EXPECT_FALSE(iframe_layout_view->NeedsPaintPropertyUpdate());
    EXPECT_TRUE(iframe_layout_view->DescendantNeedsPaintPropertyUpdate());
    EXPECT_TRUE(iframe_transform->NeedsPaintPropertyUpdate());
    EXPECT_FALSE(iframe_transform->DescendantNeedsPaintPropertyUpdate());
  }

  EXPECT_FALSE(GetDocument().View()->ShouldThrottleRendering());
  EXPECT_FALSE(ChildDocument().View()->ShouldThrottleRendering());
  // Once unthrottled, a lifecycel update should update all properties.
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(transform->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframe_layout_view->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframe_layout_view->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframe_transform->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(iframe_transform->DescendantNeedsPaintPropertyUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, ClipChangesUpdateOverflowClip) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin:0 }
      #div { overflow:hidden; height:0px; }
    </style>
    <div id='div'>
      <div style='width: 100px; height: 100px'></div>
    </div>
  )HTML");
  auto* div = GetDocument().getElementById("div");
  div->setAttribute(HTMLNames::styleAttr, "display:inline-block; width:7px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 0), clip_properties->ClipRect().Rect());

  // Width changes should update the overflow clip.
  div->setAttribute(HTMLNames::styleAttr, "display:inline-block; width:7px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 0), clip_properties->ClipRect().Rect());
  div->setAttribute(HTMLNames::styleAttr, "display:inline-block; width:9px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 9, 0), clip_properties->ClipRect().Rect());

  // An inline block's overflow clip should be updated when padding changes,
  // even if the border box remains unchanged.
  div->setAttribute(HTMLNames::styleAttr,
                    "display:inline-block; width:7px; padding-right:3px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 10, 0), clip_properties->ClipRect().Rect());
  div->setAttribute(HTMLNames::styleAttr,
                    "display:inline-block; width:8px; padding-right:2px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 10, 0), clip_properties->ClipRect().Rect());
  div->setAttribute(HTMLNames::styleAttr,
                    "display:inline-block; width:8px;"
                    "padding-right:1px; padding-left:1px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 10, 0), clip_properties->ClipRect().Rect());

  // An block's overflow clip should be updated when borders change.
  div->setAttribute(HTMLNames::styleAttr, "border-right:3px solid red;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 797, 0), clip_properties->ClipRect().Rect());
  div->setAttribute(HTMLNames::styleAttr, "border-right:5px solid red;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(FloatRect(0, 0, 795, 0), clip_properties->ClipRect().Rect());

  // Removing overflow clip should remove the property.
  div->setAttribute(HTMLNames::styleAttr, "overflow:hidden;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 800, 0), clip_properties->ClipRect().Rect());
  div->setAttribute(HTMLNames::styleAttr, "overflow:visible;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(!div->GetLayoutObject()->FirstFragment().PaintProperties() ||
              !div->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->OverflowClip());
}

TEST_P(PaintPropertyTreeUpdateTest, ContainPaintChangesUpdateOverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin:0 }
      #div { will-change:transform; width:7px; height:6px; }
    </style>
    <div id='div' style='contain:paint;'>
      <div style='width: 100px; height: 100px'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();
  auto* div = GetDocument().getElementById("div");
  auto* properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 6), properties->ClipRect().Rect());

  div->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(!div->GetLayoutObject()->FirstFragment().PaintProperties() ||
              !div->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->OverflowClip());
}

// A basic sanity check for over-invalidation of paint properties.
TEST_P(PaintPropertyTreeUpdateTest, NoPaintPropertyUpdateOnBackgroundChange) {
  SetBodyInnerHTML("<div id='div' style='background-color: blue'>DIV</div>");
  auto* div = GetDocument().getElementById("div");

  GetDocument().View()->UpdateAllLifecyclePhases();
  div->setAttribute(HTMLNames::styleAttr, "background-color: green");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());
}

// Disabled due to stale scrollsOverflow values, see: https://crbug.com/675296.
TEST_P(PaintPropertyTreeUpdateTest,
       DISABLED_FrameVisibilityChangeUpdatesProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='iframeContainer'>
      <iframe id='iframe' style='width: 100px; height: 100px;'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; }</style>"
      "<div id='forceScroll' style='height: 3000px;'></div>");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, DocScroll());
  Document* child_doc = &ChildDocument();
  EXPECT_NE(nullptr, DocScroll(child_doc));

  auto* iframe_container = GetDocument().getElementById("iframeContainer");
  iframe_container->setAttribute(HTMLNames::styleAttr, "visibility: hidden;");
  frame_view->UpdateAllLifecyclePhases();

  EXPECT_EQ(nullptr, DocScroll());
  EXPECT_EQ(nullptr, DocScroll(child_doc));
}

TEST_P(PaintPropertyTreeUpdateTest,
       TransformNodeWithAnimationLosesNodeWhenAnimationRemoved) {
  LoadTestData("transform-animation.html");
  Element* target = GetDocument().getElementById("target");
  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_TRUE(properties->Transform()->HasDirectCompositingReasons());

  // Removing the animation should remove the transform node.
  target->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  // Ensure the paint properties object was cleared as it is no longer needed.
  EXPECT_EQ(nullptr,
            target->GetLayoutObject()->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeUpdateTest,
       EffectNodeWithAnimationLosesNodeWhenAnimationRemoved) {
  LoadTestData("opacity-animation.html");
  Element* target = GetDocument().getElementById("target");
  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_TRUE(properties->Effect()->HasDirectCompositingReasons());

  // Removing the animation should remove the effect node.
  target->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr,
            target->GetLayoutObject()->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeUpdateTest,
       TransformNodeDoesNotLoseCompositorElementIdWhenAnimationRemoved) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  LoadTestData("transform-animation.html");

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "transform: translateX(2em)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());

  // Remove the animation but keep the transform on the element.
  target->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeUpdateTest,
       EffectNodeDoesNotLoseCompositorElementIdWhenAnimationRemoved) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  LoadTestData("opacity-animation.html");

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "opacity: 0.2");
  GetDocument().View()->UpdateAllLifecyclePhases();

  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());

  target->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeUpdateTest, PerspectiveOriginUpdatesOnSizeChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      #perspective {
        position: absolute;
        perspective: 100px;
        width: 100px;
        perspective-origin: 50% 50% 0;
      }
    </style>
    <div id='perspective'>
      <div id='contents'></div>
    </div>
  )HTML");

  auto* perspective = GetLayoutObjectByElementId("perspective");
  EXPECT_EQ(
      TransformationMatrix().ApplyPerspective(100),
      perspective->FirstFragment().PaintProperties()->Perspective()->Matrix());
  EXPECT_EQ(
      FloatPoint3D(50, 0, 0),
      perspective->FirstFragment().PaintProperties()->Perspective()->Origin());

  auto* contents = GetDocument().getElementById("contents");
  contents->setAttribute(HTMLNames::styleAttr, "height: 200px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(
      TransformationMatrix().ApplyPerspective(100),
      perspective->FirstFragment().PaintProperties()->Perspective()->Matrix());
  EXPECT_EQ(
      FloatPoint3D(50, 100, 0),
      perspective->FirstFragment().PaintProperties()->Perspective()->Origin());
}

TEST_P(PaintPropertyTreeUpdateTest, TransformUpdatesOnRelativeLengthChanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      #transform {
        transform: translate3d(50%, 50%, 0);
        width: 100px;
        height: 200px;
      }
    </style>
    <div id='transform'></div>
  )HTML");

  auto* transform = GetDocument().getElementById("transform");
  auto* transform_object = transform->GetLayoutObject();
  EXPECT_EQ(TransformationMatrix().Translate3d(50, 100, 0),
            transform_object->FirstFragment()
                .PaintProperties()
                ->Transform()
                ->Matrix());

  transform->setAttribute(HTMLNames::styleAttr, "width: 200px; height: 300px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix().Translate3d(100, 150, 0),
            transform_object->FirstFragment()
                .PaintProperties()
                ->Transform()
                ->Matrix());
}

TEST_P(PaintPropertyTreeUpdateTest, CSSClipDependingOnSize) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      #outer {
        position: absolute;
        width: 100px; height: 100px; top: 50px; left: 50px;
      }
      #clip {
        position: absolute;
        clip: rect(auto auto auto -5px);
        top: 0; left: 0; right: 0; bottom: 0;
      }
    </style>
    <div id='outer'>
      <div id='clip'></div>
    </div>
  )HTML");

  auto* outer = GetDocument().getElementById("outer");
  auto* clip = GetLayoutObjectByElementId("clip");
  EXPECT_EQ(
      FloatRect(45, 50, 105, 100),
      clip->FirstFragment().PaintProperties()->CssClip()->ClipRect().Rect());

  outer->setAttribute(HTMLNames::styleAttr, "height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(
      FloatRect(45, 50, 105, 200),
      clip->FirstFragment().PaintProperties()->CssClip()->ClipRect().Rect());
}

TEST_P(PaintPropertyTreeUpdateTest, ScrollBoundsChange) {
  SetBodyInnerHTML(R"HTML(
    <div id='container'
        style='width: 100px; height: 100px; overflow: scroll'>
      <div id='content' style='width: 200px; height: 200px'></div>
    </div>
  )HTML");

  auto* container = GetLayoutObjectByElementId("container");
  auto* scroll_node = container->FirstFragment()
                          .PaintProperties()
                          ->ScrollTranslation()
                          ->ScrollNode();
  EXPECT_EQ(IntRect(0, 0, 100, 100), scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(200, 200), scroll_node->ContentsSize());

  GetDocument().getElementById("content")->setAttribute(
      HTMLNames::styleAttr, "width: 200px; height: 300px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(scroll_node, container->FirstFragment()
                             .PaintProperties()
                             ->ScrollTranslation()
                             ->ScrollNode());
  EXPECT_EQ(IntRect(0, 0, 100, 100), scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(200, 300), scroll_node->ContentsSize());
}

// The scrollbars are attached to the visual viewport but created by (and have
// space saved by) the frame view. Conceptually, the scrollbars are part of the
// scrollable content so they must be included in the contents rect. They must
// also not be excluded from the container rect since they don't take away space
// from the viewport's viewable area.
TEST_P(PaintPropertyTreeUpdateTest,
       ViewportContentsAndContainerRectsIncludeScrollbar) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar {width: 20px; height: 20px}
      body {height: 10000px; width: 10000px; margin: 0;}
    </style>
  )HTML");

  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();

  EXPECT_EQ(IntRect(0, 0, 800, 600),
            visual_viewport.GetScrollNode()->ContainerRect());
  EXPECT_EQ(IntSize(800, 600), visual_viewport.GetScrollNode()->ContentsSize());
}

TEST_P(PaintPropertyTreeUpdateTest, ScrollbarWidthChange) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar {width: 20px; height: 20px}</style>
    <div id='container'
        style='width: 100px; height: 100px; overflow: scroll'>
      <div id='content' style='width: 200px; height: 200px'></div>
    </div>
  )HTML");

  auto* container = GetLayoutObjectByElementId("container");
  auto* overflow_clip =
      container->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatSize(80, 80), overflow_clip->ClipRect().Rect().Size());

  auto* new_style = GetDocument().CreateRawElement(HTMLNames::styleTag);
  new_style->setTextContent("::-webkit-scrollbar {width: 40px; height: 40px}");
  GetDocument().body()->AppendChild(new_style);

  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(overflow_clip,
            container->FirstFragment().PaintProperties()->OverflowClip());
  EXPECT_EQ(FloatSize(60, 60), overflow_clip->ClipRect().Rect().Size());
}

TEST_P(PaintPropertyTreeUpdateTest, Preserve3DChange) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent'>
      <div id='child' style='transform: translate3D(1px, 2px, 3px)'></div>
    </div>
  )HTML");

  auto* child = GetLayoutObjectByElementId("child");
  auto* transform = child->FirstFragment().PaintProperties()->Transform();
  EXPECT_TRUE(transform->FlattensInheritedTransform());

  GetDocument().getElementById("parent")->setAttribute(
      HTMLNames::styleAttr, "transform-style: preserve-3d");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(transform, child->FirstFragment().PaintProperties()->Transform());
  EXPECT_FALSE(transform->FlattensInheritedTransform());
}

TEST_P(PaintPropertyTreeUpdateTest, MenuListControlClipChange) {
  SetBodyInnerHTML(R"HTML(
    <select id='select' style='white-space: normal'>
      <option></option>
      <option>bar</option>
    </select>
  )HTML");

  auto* select = GetLayoutObjectByElementId("select");
  EXPECT_NE(nullptr, select->FirstFragment().PaintProperties()->OverflowClip());

  // Should not assert in FindPropertiesNeedingUpdate.
  ToHTMLSelectElement(select->GetNode())->setSelectedIndex(1);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_NE(nullptr, select->FirstFragment().PaintProperties()->OverflowClip());
}

TEST_P(PaintPropertyTreeUpdateTest, BoxAddRemoveMask) {
  SetBodyInnerHTML(R"HTML(
    <style>#target {width: 100px; height: 100px}</style>
    <div id='target'>
      <div style='width:500px; height:500px; background:green;'></div>
    </div>
  )HTML");

  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  auto* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr,
                       "-webkit-mask: linear-gradient(red, blue)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 100), mask_clip->ClipRect());

  target->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));
}

TEST_P(PaintPropertyTreeUpdateTest, MaskClipNodeBoxSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #target {
      width: 100px;
      height: 100px;
      -webkit-mask: linear-gradient(red, blue);
    }
    </style>
    <div id='target'>
      <div style='width:500px; height:500px; background:green;'></div>
    </div>
  )HTML");

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 100), mask_clip->ClipRect());

  GetDocument().getElementById("target")->setAttribute(HTMLNames::styleAttr,
                                                       "height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_EQ(mask_clip, properties->MaskClip());
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 200), mask_clip->ClipRect());
}

TEST_P(PaintPropertyTreeUpdateTest, InlineAddRemoveMask) {
  SetBodyInnerHTML(
      "<span id='target'><img id='img' style='width: 50px'></span>");

  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  auto* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr,
                       "-webkit-mask: linear-gradient(red, blue)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(50, mask_clip->ClipRect().Rect().Width());

  target->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));
}

TEST_P(PaintPropertyTreeUpdateTest, MaskClipNodeInlineBoundsChange) {
  SetBodyInnerHTML(R"HTML(
    <span id='target' style='-webkit-mask: linear-gradient(red, blue)'>
      <img id='img' style='width: 50px'>
    </span>
  )HTML");

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(50, mask_clip->ClipRect().Rect().Width());

  GetDocument().getElementById("img")->setAttribute(HTMLNames::styleAttr,
                                                    "width: 100px");
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_EQ(mask_clip, properties->MaskClip());
  EXPECT_EQ(100, mask_clip->ClipRect().Rect().Width());
}

TEST_P(PaintPropertyTreeUpdateTest, AddRemoveSVGMask) {
  SetBodyInnerHTML(R"HTML(
    <svg width='200' height='200'>
      <rect id='rect' x='0' y='100' width='100' height='100' fill='blue'/>
      <defs>
        <mask id='mask' x='0' y='0' width='100' height='200'>
          <rect width='100' height='200' fill='red'/>
        </mask>
      </defs>
    </svg>
  )HTML");

  EXPECT_EQ(nullptr, PaintPropertiesForElement("rect"));

  GetDocument().getElementById("rect")->setAttribute("mask", "url(#mask)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto* properties = PaintPropertiesForElement("rect");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(FloatRoundedRect(0, 100, 100, 100), mask_clip->ClipRect());

  GetDocument().getElementById("rect")->removeAttribute("mask");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("rect"));
}

TEST_P(PaintPropertyTreeUpdateTest, SVGMaskTargetBoundsChange) {
  SetBodyInnerHTML(R"HTML(
    <svg width='500' height='500'>
      <g id='target' mask='url(#mask)'>
        <rect id='rect' x='0' y='50' width='50' height='100' fill='blue'/>
      </g>
      <defs>
        <mask id='mask' x='0' y='0' width='100' height='200'>
          <rect width='100' height='200' fill='red'/>
        </mask>
      </defs>
    </svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(FloatRoundedRect(0, 50, 100, 150), mask_clip->ClipRect());

  GetDocument().getElementById("rect")->setAttribute("width", "200");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  EXPECT_EQ(FloatRoundedRect(0, 50, 100, 150), mask_clip->ClipRect());
}

TEST_P(PaintPropertyTreeUpdateTest, WillTransformChangeAboveFixed) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container { position: absolute; top: 100px; left: 100px }
    </style>
    <div id='container' style='will-change: transform'>
      <div id='fixed' style='position: fixed; top: 50px; left: 50px'></div>
    </div>
  )HTML");

  const auto* container = GetLayoutObjectByElementId("container");
  const auto* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(container->FirstFragment().PaintProperties()->Transform(),
            fixed->FirstFragment().LocalBorderBoxProperties().Transform());

  ToElement(container->GetNode())
      ->setAttribute(HTMLNames::styleAttr, "will-change: top");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(
      GetLayoutView().FirstFragment().LocalBorderBoxProperties().Transform(),
      fixed->FirstFragment().LocalBorderBoxProperties().Transform());

  ToElement(container->GetNode())
      ->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(container->FirstFragment().PaintProperties()->Transform(),
            fixed->FirstFragment().LocalBorderBoxProperties().Transform());
}

TEST_P(PaintPropertyTreeUpdateTest, CompositingReasonForAnimation) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        transition: 100s;
        filter: opacity(30%);
        transform: translateX(10px);
        position: relative;
      }
    </style>
    <div id='target'>TARGET</div>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  auto* transform =
      target->GetLayoutObject()->FirstFragment().PaintProperties()->Transform();
  ASSERT_TRUE(transform);
  EXPECT_FALSE(transform->HasDirectCompositingReasons());

  auto* filter =
      target->GetLayoutObject()->FirstFragment().PaintProperties()->Filter();
  ASSERT_TRUE(filter);
  EXPECT_FALSE(filter->HasDirectCompositingReasons());

  target->setAttribute(HTMLNames::styleAttr, "transform: translateX(11px)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(transform->HasDirectCompositingReasons());
    EXPECT_TRUE(transform->RequiresCompositingForAnimation());
  }
  EXPECT_FALSE(filter->HasDirectCompositingReasons());

  target->setAttribute(HTMLNames::styleAttr,
                       "transform: translateX(11px); filter: opacity(40%)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // The transform animation still continues.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(transform->HasDirectCompositingReasons());
    EXPECT_TRUE(transform->RequiresCompositingForAnimation());
    // The filter node should have correct direct compositing reasons, not
    // shadowed by the transform animation.
    EXPECT_TRUE(filter->HasDirectCompositingReasons());
    EXPECT_TRUE(filter->RequiresCompositingForAnimation());
  }
}

TEST_P(PaintPropertyTreeUpdateTest, SVGViewportContainerOverflowChange) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <svg id='target' width='30' height='40'></svg>
    </svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_EQ(FloatRect(0, 0, 30, 40),
            properties->OverflowClip()->ClipRect().Rect());

  GetDocument().getElementById("target")->setAttribute("overflow", "visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  GetDocument().getElementById("target")->setAttribute("overflow", "hidden");
  GetDocument().View()->UpdateAllLifecyclePhases();
  properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_EQ(FloatRect(0, 0, 30, 40),
            properties->OverflowClip()->ClipRect().Rect());
}

TEST_P(PaintPropertyTreeUpdateTest, SVGForeignObjectOverflowChange) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <foreignObject id='target' x='10' y='20' width='30' height='40'
          overflow='hidden'>
      </foreignObject>
    </svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_EQ(FloatRect(10, 20, 30, 40),
            properties->OverflowClip()->ClipRect().Rect());

  GetDocument().getElementById("target")->setAttribute("overflow", "visible");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  GetDocument().getElementById("target")->setAttribute("overflow", "hidden");
  GetDocument().View()->UpdateAllLifecyclePhases();
  properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_EQ(FloatRect(10, 20, 30, 40),
            properties->OverflowClip()->ClipRect().Rect());
}

TEST_P(PaintPropertyTreeBuilderTest, OmitOverflowClipOnSelectionChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="overflow: hidden">
      <img style="width: 50px; height: 50px">
    </div>
  )HTML");

  EXPECT_FALSE(PaintPropertiesForElement("target")->OverflowClip());

  GetDocument().GetFrame()->Selection().SelectAll();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(PaintPropertiesForElement("target")->OverflowClip());

  GetDocument().GetFrame()->Selection().Clear();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(PaintPropertiesForElement("target")->OverflowClip());
}

TEST_P(PaintPropertyTreeBuilderTest, OmitOverflowClipOnCaretChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" contentEditable="true" style="overflow: hidden">
      <img style="width: 50px; height: 50px">
    </div>
  )HTML");

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* target = GetDocument().getElementById("target");
  EXPECT_FALSE(PaintPropertiesForElement("target")->OverflowClip());

  target->focus();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(PaintPropertiesForElement("target")->OverflowClip());

  target->blur();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(PaintPropertiesForElement("target")->OverflowClip());
}

TEST_P(PaintPropertyTreeUpdateTest,
       FragmentClipUpdateOnMulticolContainerWidthChange) {
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <div id="container" style="width: 100px">
      <div id="multicol" style="columns: 2; column-gap: 0; line-height: 500px">
        <div><br></div>
        <div><br></div>
      </div>
    </div>
  )HTML");

  auto* flow_thread = GetLayoutObjectByElementId("multicol")->SlowFirstChild();
  ASSERT_EQ(2u, NumFragments(flow_thread));
  EXPECT_EQ(1000000, FragmentAt(flow_thread, 0)
                         .PaintProperties()
                         ->FragmentClip()
                         ->ClipRect()
                         .Rect()
                         .MaxX());
  EXPECT_EQ(-999950, FragmentAt(flow_thread, 1)
                         .PaintProperties()
                         ->FragmentClip()
                         ->ClipRect()
                         .Rect()
                         .X());

  GetDocument()
      .getElementById("container")
      ->setAttribute(HTMLNames::styleAttr, "width: 500px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_EQ(2u, NumFragments(flow_thread));
  EXPECT_EQ(1000000, FragmentAt(flow_thread, 0)
                         .PaintProperties()
                         ->FragmentClip()
                         ->ClipRect()
                         .Rect()
                         .MaxX());
  EXPECT_EQ(-999750, FragmentAt(flow_thread, 1)
                         .PaintProperties()
                         ->FragmentClip()
                         ->ClipRect()
                         .Rect()
                         .X());
}

TEST_P(PaintPropertyTreeUpdateTest,
       PropertyTreesRebuiltAfterSVGBlendModeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #blended {
        mix-blend-mode: darken;
        fill: red;
      }
    </style>
    <svg width="100" height="100">
      <rect id="blended" x="0" y="0" width="100" height="100"></rect>
    </svg>
  )HTML");

  auto* blended_element = GetDocument().getElementById("blended");
  ASSERT_TRUE(blended_element);
  const auto* props =
      blended_element->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_TRUE(props->Effect());
  EXPECT_EQ(props->Effect()->BlendMode(), SkBlendMode::kDarken);

  blended_element->setAttribute(HTMLNames::styleAttr,
                                "mix-blend-mode: lighten;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  props = blended_element->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_TRUE(props->Effect());
  EXPECT_EQ(props->Effect()->BlendMode(), SkBlendMode::kLighten);
}

TEST_P(PaintPropertyTreeUpdateTest, EnsureSnapContainerData) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      overflow: scroll;
      scroll-snap-type: both proximity;
      height: 300px;
      width: 300px;
      margin: 0px;
      padding: 0px;
    }
    #container {
      margin: 0px;
      padding: 0px;
      width: 600px;
      height: 2000px;
    }
    #area {
      position: relative;
      left: 100px;
      top: 700px;
      width: 200px;
      height: 200px;
      scroll-snap-align: start;
    }

    </style>

    <div id="container">
      <div id="area"></div>
    </div>
  )HTML");

  GetDocument().View()->Resize(300, 300);
  GetDocument().View()->UpdateAllLifecyclePhases();

  auto doc_snap_container_data = DocScroll()->GetSnapContainerData();
  ASSERT_TRUE(doc_snap_container_data);
  EXPECT_EQ(doc_snap_container_data->scroll_snap_type().axis, SnapAxis::kBoth);
  EXPECT_EQ(doc_snap_container_data->scroll_snap_type().strictness,
            SnapStrictness::kProximity);
  EXPECT_EQ(doc_snap_container_data->rect(), gfx::RectF(0, 0, 300, 300));
  EXPECT_EQ(doc_snap_container_data->size(), 1u);
  EXPECT_EQ(doc_snap_container_data->at(0).rect,
            gfx::RectF(100, 700, 200, 200));
}

TEST_P(PaintPropertyTreeUpdateTest,
       EffectAndClipWithNonContainedOutOfFlowDescendant) {
  SetBodyInnerHTML(R"HTML(
    <div id="clip" style="overflow: hidden; width: 100px; height: 100px">
      <div id="effect" style="opacity: 0.5">
        <div id="descendant" style="position: fixed">Fixed</div>
      </div>
    </div>
  )HTML");

  const auto* clip_properties = PaintPropertiesForElement("clip");
  EXPECT_NE(nullptr, clip_properties->OverflowClip());
  const auto* effect_properties = PaintPropertiesForElement("effect");
  ASSERT_NE(nullptr, effect_properties->Effect());
  // The effect's OutputClip is nullptr because of the fixed descendant.
  EXPECT_EQ(nullptr, effect_properties->Effect()->OutputClip());

  auto* descendant = GetDocument().getElementById("descendant");
  descendant->setAttribute(HTMLNames::styleAttr, "position: relative");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(clip_properties->OverflowClip(),
            effect_properties->Effect()->OutputClip());

  descendant->setAttribute(HTMLNames::styleAttr, "position: absolute");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // The effect's OutputClip is nullptr because of the absolute descendant.
  EXPECT_EQ(nullptr, effect_properties->Effect()->OutputClip());
}

TEST_P(PaintPropertyTreeUpdateTest, ForwardReferencedSVGElementUpdate) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg1" filter="url(#filter)">
      <filter id="filter">
        <feImage id="image" href="#rect"/>
      </filter>
    </svg>
    <svg id="svg2" style="perspective: 10px">
      <rect id="rect" width="100" height="100" transform="translate(1)"/>
    </svg>
  )HTML");

  const auto* svg2_properties = PaintPropertiesForElement("svg2");
  EXPECT_NE(nullptr, svg2_properties->PaintOffsetTranslation());
  EXPECT_EQ(nullptr, svg2_properties->Transform());
  EXPECT_NE(nullptr, svg2_properties->Perspective());
  EXPECT_EQ(svg2_properties->PaintOffsetTranslation(),
            svg2_properties->Perspective()->Parent());

  const auto* rect_properties = PaintPropertiesForElement("rect");
  ASSERT_NE(nullptr, rect_properties->Transform());
  EXPECT_EQ(svg2_properties->Perspective(),
            rect_properties->Transform()->Parent());
  EXPECT_EQ(TransformationMatrix().Translate(1, 0),
            GeometryMapper::SourceToDestinationProjection(
                rect_properties->Transform(),
                svg2_properties->PaintOffsetTranslation()));

  // Change filter which forward references rect, and insert a transform
  // node above rect's transform.
  GetDocument().getElementById("filter")->setAttribute("width", "20");
  GetDocument().getElementById("svg2")->setAttribute("transform",
                                                     "translate(2)");
  UpdateAllLifecyclePhases();

  EXPECT_NE(nullptr, svg2_properties->Transform());
  EXPECT_EQ(svg2_properties->PaintOffsetTranslation(),
            svg2_properties->Transform()->Parent());
  EXPECT_EQ(svg2_properties->Transform(),
            svg2_properties->Perspective()->Parent());
  EXPECT_EQ(svg2_properties->Perspective(),
            rect_properties->Transform()->Parent());

  // Ensure that GeometryMapper's cache is properly invalidated and updated.
  EXPECT_EQ(TransformationMatrix().Translate(3, 0),
            GeometryMapper::SourceToDestinationProjection(
                rect_properties->Transform(),
                svg2_properties->PaintOffsetTranslation()));
}

}  // namespace blink
