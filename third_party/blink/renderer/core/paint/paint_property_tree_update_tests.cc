// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder_test.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

// Tests covering incremental updates of paint property trees.
class PaintPropertyTreeUpdateTest : public PaintPropertyTreeBuilderTest {};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintPropertyTreeUpdateTest);

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
  UpdateAllLifecyclePhasesForTest();

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
  UpdateAllLifecyclePhasesForTest();
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
  overflow_b->setAttribute(html_names::kClassAttr, "backgroundAttachmentFixed");
  UpdateAllLifecyclePhasesForTest();
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
  UpdateAllLifecyclePhasesForTest();
  Document* parent = &GetDocument();
  EXPECT_TRUE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  Document* child = &ChildDocument();
  EXPECT_TRUE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  auto* fixed_background = GetDocument().getElementById("fixedBackground");
  fixed_background->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  fixed_background->setAttribute(html_names::kClassAttr, "fixedBackground");
  UpdateAllLifecyclePhasesForTest();
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
  UpdateAllLifecyclePhasesForTest();

  Document* parent = &GetDocument();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  Document* child = &ChildDocument();
  EXPECT_TRUE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Removing a main thread scrolling reason should update the entire tree.
  auto* fixed_background = ChildDocument().getElementById("fixedBackground");
  fixed_background->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(DocScroll(parent)->HasBackgroundAttachmentFixedDescendants());
  EXPECT_FALSE(DocScroll(child)->HasBackgroundAttachmentFixedDescendants());

  // Adding a main thread scrolling reason should update the entire tree.
  fixed_background->setAttribute(html_names::kClassAttr, "fixedBackground");
  UpdateAllLifecyclePhasesForTest();
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

  // This should be false. We are not as strict about main thread scrolling
  // reasons as we could be.
  EXPECT_TRUE(overflow_a->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());
  // This could be false since it's fixed with respect to the layout viewport.
  // However, it would be simpler to avoid the main thread by doing this check
  // on the compositor thread. https://crbug.com/985127.
  EXPECT_TRUE(overflow_b->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ScrollTranslation()
                  ->ScrollNode()
                  ->HasBackgroundAttachmentFixedDescendants());
  EXPECT_EQ(DocScroll(), overflow_b->GetLayoutObject()
                             ->FirstFragment()
                             .PaintProperties()
                             ->ScrollTranslation()
                             ->ScrollNode()
                             ->Parent());

  // Removing a main thread scrolling reason should update the entire tree.
  overflow_b->removeAttribute("class");
  UpdateAllLifecyclePhasesForTest();
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
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

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
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
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
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      child_frame_view->GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  EXPECT_FALSE(child_frame_view->GetLayoutView()->NeedsPaintPropertyUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, UpdatingFrameViewContentClip) {
  SetBodyInnerHTML("hello world.");
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600),
            DocContentClip()->UnsnappedClipRect());
  GetDocument().View()->Resize(800, 599);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 599),
            DocContentClip()->UnsnappedClipRect());
  GetDocument().View()->Resize(800, 600);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600),
            DocContentClip()->UnsnappedClipRect());
  GetDocument().View()->Resize(5, 5);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRoundedRect(0, 0, 5, 5),
            DocContentClip()->UnsnappedClipRect());
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
  auto* iframe = To<HTMLIFrameElement>(GetDocument().getElementById("iframe"));
  iframe->setAttribute(html_names::kStyleAttr, "transform: translateY(5555px)");
  UpdateAllLifecyclePhasesForTest();
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
    UpdateAllLifecyclePhasesForTest();
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
  UpdateAllLifecyclePhasesForTest();
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
  div->setAttribute(html_names::kStyleAttr, "display:inline-block; width:7px;");
  UpdateAllLifecyclePhasesForTest();
  auto* clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 0), clip_properties->UnsnappedClipRect().Rect());

  // Width changes should update the overflow clip.
  div->setAttribute(html_names::kStyleAttr, "display:inline-block; width:7px;");
  UpdateAllLifecyclePhasesForTest();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 0), clip_properties->UnsnappedClipRect().Rect());
  div->setAttribute(html_names::kStyleAttr, "display:inline-block; width:9px;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRect(0, 0, 9, 0), clip_properties->UnsnappedClipRect().Rect());

  // An inline block's overflow clip should be updated when padding changes,
  // even if the border box remains unchanged.
  div->setAttribute(html_names::kStyleAttr,
                    "display:inline-block; width:7px; padding-right:3px;");
  UpdateAllLifecyclePhasesForTest();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 10, 0),
            clip_properties->UnsnappedClipRect().Rect());
  div->setAttribute(html_names::kStyleAttr,
                    "display:inline-block; width:8px; padding-right:2px;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRect(0, 0, 10, 0),
            clip_properties->UnsnappedClipRect().Rect());
  div->setAttribute(html_names::kStyleAttr,
                    "display:inline-block; width:8px;"
                    "padding-right:1px; padding-left:1px;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRect(0, 0, 10, 0),
            clip_properties->UnsnappedClipRect().Rect());

  // An block's overflow clip should be updated when borders change.
  div->setAttribute(html_names::kStyleAttr, "border-right:3px solid red;");
  UpdateAllLifecyclePhasesForTest();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 797, 0),
            clip_properties->UnsnappedClipRect().Rect());
  div->setAttribute(html_names::kStyleAttr, "border-right:5px solid red;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRect(0, 0, 795, 0),
            clip_properties->UnsnappedClipRect().Rect());

  // Removing overflow clip should remove the property.
  div->setAttribute(html_names::kStyleAttr, "overflow:hidden;");
  UpdateAllLifecyclePhasesForTest();
  clip_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 800, 0),
            clip_properties->UnsnappedClipRect().Rect());
  div->setAttribute(html_names::kStyleAttr, "overflow:visible;");
  UpdateAllLifecyclePhasesForTest();
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
  UpdateAllLifecyclePhasesForTest();
  auto* div = GetDocument().getElementById("div");
  auto* properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties()->OverflowClip();
  EXPECT_EQ(FloatRect(0, 0, 7, 6), properties->UnsnappedClipRect().Rect());

  div->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhasesForTest();
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

  UpdateAllLifecyclePhasesForTest();
  div->setAttribute(html_names::kStyleAttr, "background-color: green");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
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
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  EXPECT_EQ(nullptr, DocScroll());
  Document* child_doc = &ChildDocument();
  EXPECT_NE(nullptr, DocScroll(child_doc));

  auto* iframe_container = GetDocument().getElementById("iframeContainer");
  iframe_container->setAttribute(html_names::kStyleAttr, "visibility: hidden;");
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  EXPECT_EQ(nullptr, DocScroll());
  EXPECT_EQ(nullptr, DocScroll(child_doc));
}

TEST_P(PaintPropertyTreeUpdateTest,
       TransformNodeWithAnimationLosesNodeWhenAnimationRemoved) {
  LoadTestData("transform-animation.html");
  Element* target = GetDocument().getElementById("target");
  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    EXPECT_TRUE(properties->Transform()->HasDirectCompositingReasons());

  // Removing the animation should remove the transform node.
  target->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
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
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    EXPECT_TRUE(properties->Effect()->HasDirectCompositingReasons());

  // Removing the animation should remove the effect node.
  target->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(nullptr,
            target->GetLayoutObject()->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeUpdateTest,
       TransformNodeDoesNotLoseCompositorElementIdWhenAnimationRemoved) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  LoadTestData("transform-animation.html");

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr, "transform: translateX(2em)");
  UpdateAllLifecyclePhasesForTest();

  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());

  // Remove the animation but keep the transform on the element.
  target->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeUpdateTest,
       EffectNodeDoesNotLoseCompositorElementIdWhenAnimationRemoved) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  LoadTestData("opacity-animation.html");

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr, "opacity: 0.2");
  UpdateAllLifecyclePhasesForTest();

  const ObjectPaintProperties* properties =
      target->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());

  target->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
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
  contents->setAttribute(html_names::kStyleAttr, "height: 200px;");
  UpdateAllLifecyclePhasesForTest();
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
  EXPECT_EQ(FloatSize(50, 100), transform_object->FirstFragment()
                                    .PaintProperties()
                                    ->Transform()
                                    ->Translation2D());

  transform->setAttribute(html_names::kStyleAttr,
                          "width: 200px; height: 300px;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatSize(100, 150), transform_object->FirstFragment()
                                     .PaintProperties()
                                     ->Transform()
                                     ->Translation2D());
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
  EXPECT_EQ(FloatRect(45, 50, 105, 100), clip->FirstFragment()
                                             .PaintProperties()
                                             ->CssClip()
                                             ->UnsnappedClipRect()
                                             .Rect());

  outer->setAttribute(html_names::kStyleAttr, "height: 200px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(FloatRect(45, 50, 105, 200), clip->FirstFragment()
                                             .PaintProperties()
                                             ->CssClip()
                                             ->UnsnappedClipRect()
                                             .Rect());
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
      html_names::kStyleAttr, "width: 200px; height: 300px");
  UpdateAllLifecyclePhasesForTest();
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

TEST_P(PaintPropertyTreeUpdateTest, ViewportAddRemoveDeviceEmulationNode) {
  SetBodyInnerHTML(
      "<style>body {height: 10000px; width: 10000px; margin: 0;}</style>");

  auto& visual_viewport = GetDocument().GetPage()->GetVisualViewport();
  EXPECT_FALSE(visual_viewport.GetDeviceEmulationTransformNode());
  // The LayoutView (instead of VisualViewport) creates scrollbars because
  // viewport is disabled.
  ASSERT_FALSE(GetDocument().GetPage()->GetSettings().GetViewportEnabled());
  EXPECT_FALSE(visual_viewport.LayerForHorizontalScrollbar());
  EXPECT_FALSE(visual_viewport.LayerForVerticalScrollbar());
  ASSERT_TRUE(GetLayoutView().GetScrollableArea());
  auto* scrollbar_layer = GetLayoutView()
                              .GetScrollableArea()
                              ->GraphicsLayerForHorizontalScrollbar();
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    ASSERT_TRUE(scrollbar_layer);
    EXPECT_EQ(&TransformPaintPropertyNode::Root(),
              &scrollbar_layer->GetPropertyTreeState().Transform());
  } else {
    // TODO(wangxianzhu): Test for CompositeAfterPaint.
    EXPECT_FALSE(scrollbar_layer);
  }

  // These emulate WebViewImpl::SetDeviceEmulationTransform().
  GetChromeClient().SetDeviceEmulationTransform(
      TransformationMatrix().Scale(2));
  visual_viewport.SetNeedsPaintPropertyUpdate();

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(visual_viewport.GetDeviceEmulationTransformNode());
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    ASSERT_TRUE(scrollbar_layer);
    EXPECT_EQ(visual_viewport.GetDeviceEmulationTransformNode(),
              &scrollbar_layer->GetPropertyTreeState().Transform());
  } else {
    // TODO(wangxianzhu): Test for CompositeAfterPaint.
    EXPECT_FALSE(scrollbar_layer);
  }

  // These emulate WebViewImpl::SetDeviceEmulationTransform().
  GetChromeClient().SetDeviceEmulationTransform(TransformationMatrix());
  visual_viewport.SetNeedsPaintPropertyUpdate();

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(visual_viewport.GetDeviceEmulationTransformNode());
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    ASSERT_TRUE(scrollbar_layer);
    EXPECT_EQ(&TransformPaintPropertyNode::Root(),
              &scrollbar_layer->GetPropertyTreeState().Transform());
  } else {
    // TODO(wangxianzhu): Test for CompositeAfterPaint.
    EXPECT_FALSE(scrollbar_layer);
  }
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
  EXPECT_EQ(FloatSize(80, 80),
            overflow_clip->UnsnappedClipRect().Rect().Size());

  auto* new_style = GetDocument().CreateRawElement(html_names::kStyleTag);
  new_style->setTextContent("::-webkit-scrollbar {width: 40px; height: 40px}");
  GetDocument().body()->AppendChild(new_style);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(overflow_clip,
            container->FirstFragment().PaintProperties()->OverflowClip());
  EXPECT_EQ(FloatSize(60, 60),
            overflow_clip->UnsnappedClipRect().Rect().Size());
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
      html_names::kStyleAttr, "transform-style: preserve-3d");
  UpdateAllLifecyclePhasesForTest();
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
  To<HTMLSelectElement>(select->GetNode())->setSelectedIndex(1);
  UpdateAllLifecyclePhasesForTest();
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
  target->setAttribute(html_names::kStyleAttr,
                       "-webkit-mask: linear-gradient(red, blue)");
  UpdateAllLifecyclePhasesForTest();

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 100), mask_clip->UnsnappedClipRect());

  target->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhasesForTest();
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
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 100), mask_clip->UnsnappedClipRect());

  GetDocument().getElementById("target")->setAttribute(html_names::kStyleAttr,
                                                       "height: 200px");
  UpdateAllLifecyclePhasesForTest();

  ASSERT_EQ(mask_clip, properties->MaskClip());
  EXPECT_EQ(FloatRoundedRect(8, 8, 100, 200), mask_clip->UnsnappedClipRect());
}

TEST_P(PaintPropertyTreeUpdateTest, InlineAddRemoveMask) {
  SetBodyInnerHTML(
      "<span id='target'><img id='img' style='width: 50px'></span>");

  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  auto* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr,
                       "-webkit-mask: linear-gradient(red, blue)");
  UpdateAllLifecyclePhasesForTest();

  const auto* properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(50, mask_clip->UnsnappedClipRect().Rect().Width());

  target->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhasesForTest();
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
  EXPECT_EQ(50, mask_clip->UnsnappedClipRect().Rect().Width());

  GetDocument().getElementById("img")->setAttribute(html_names::kStyleAttr,
                                                    "width: 100px");
  UpdateAllLifecyclePhasesForTest();

  ASSERT_EQ(mask_clip, properties->MaskClip());
  EXPECT_EQ(100, mask_clip->UnsnappedClipRect().Rect().Width());
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
  UpdateAllLifecyclePhasesForTest();
  const auto* properties = PaintPropertiesForElement("rect");
  ASSERT_NE(nullptr, properties);
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  const auto* mask_clip = properties->MaskClip();
  ASSERT_NE(nullptr, mask_clip);
  EXPECT_EQ(FloatRoundedRect(0, 100, 100, 100), mask_clip->UnsnappedClipRect());

  GetDocument().getElementById("rect")->removeAttribute("mask");
  UpdateAllLifecyclePhasesForTest();
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
  EXPECT_EQ(FloatRoundedRect(0, 50, 100, 150), mask_clip->UnsnappedClipRect());

  GetDocument().getElementById("rect")->setAttribute("width", "200");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(nullptr, properties->Effect());
  EXPECT_NE(nullptr, properties->Mask());
  EXPECT_EQ(FloatRoundedRect(0, 50, 100, 150), mask_clip->UnsnappedClipRect());
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
            &fixed->FirstFragment().LocalBorderBoxProperties().Transform());

  To<Element>(container->GetNode())
      ->setAttribute(html_names::kStyleAttr, "will-change: top");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      &GetLayoutView().FirstFragment().LocalBorderBoxProperties().Transform(),
      &fixed->FirstFragment().LocalBorderBoxProperties().Transform());

  To<Element>(container->GetNode())
      ->setAttribute(html_names::kStyleAttr, "will-change: transform");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(container->FirstFragment().PaintProperties()->Transform(),
            &fixed->FirstFragment().LocalBorderBoxProperties().Transform());
}

TEST_P(PaintPropertyTreeUpdateTest, CompositingReasonForAnimation) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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

  target->setAttribute(html_names::kStyleAttr, "transform: translateX(11px)");
  UpdateAllLifecyclePhasesForTest();
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_TRUE(transform->HasDirectCompositingReasons());
    EXPECT_TRUE(transform->HasActiveTransformAnimation());
  }
  // TODO(flackr): After https://crbug.com/900241 is fixed the filter effect
  // should no longer have direct compositing reasons due to the animation.
  EXPECT_TRUE(filter->HasDirectCompositingReasons());

  target->setAttribute(html_names::kStyleAttr,
                       "transform: translateX(11px); filter: opacity(40%)");
  UpdateAllLifecyclePhasesForTest();
  // The transform animation still continues.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_TRUE(transform->HasDirectCompositingReasons());
    EXPECT_TRUE(transform->HasActiveTransformAnimation());
    // The filter node should have correct direct compositing reasons, not
    // shadowed by the transform animation.
    EXPECT_TRUE(filter->HasDirectCompositingReasons());
    EXPECT_TRUE(transform->HasActiveTransformAnimation());
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
            properties->OverflowClip()->UnsnappedClipRect().Rect());

  GetDocument().getElementById("target")->setAttribute("overflow", "visible");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  GetDocument().getElementById("target")->setAttribute("overflow", "hidden");
  UpdateAllLifecyclePhasesForTest();
  properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_EQ(FloatRect(0, 0, 30, 40),
            properties->OverflowClip()->UnsnappedClipRect().Rect());
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
            properties->OverflowClip()->UnsnappedClipRect().Rect());

  GetDocument().getElementById("target")->setAttribute("overflow", "visible");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target"));

  GetDocument().getElementById("target")->setAttribute("overflow", "hidden");
  UpdateAllLifecyclePhasesForTest();
  properties = PaintPropertiesForElement("target");
  ASSERT_NE(nullptr, properties);
  EXPECT_EQ(FloatRect(10, 20, 30, 40),
            properties->OverflowClip()->UnsnappedClipRect().Rect());
}

TEST_P(PaintPropertyTreeBuilderTest, OmitOverflowClipOnSelectionChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="overflow: hidden">
      <img style="width: 50px; height: 50px">
    </div>
  )HTML");

  EXPECT_FALSE(PaintPropertiesForElement("target")->OverflowClip());

  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(PaintPropertiesForElement("target")->OverflowClip());

  GetDocument().GetFrame()->Selection().Clear();
  UpdateAllLifecyclePhasesForTest();
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
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(PaintPropertiesForElement("target")->OverflowClip());

  target->blur();
  UpdateAllLifecyclePhasesForTest();
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
                         ->UnsnappedClipRect()
                         .Rect()
                         .MaxX());
  EXPECT_EQ(-999950, FragmentAt(flow_thread, 1)
                         .PaintProperties()
                         ->FragmentClip()
                         ->UnsnappedClipRect()
                         .Rect()
                         .X());

  GetDocument()
      .getElementById("container")
      ->setAttribute(html_names::kStyleAttr, "width: 500px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(2u, NumFragments(flow_thread));
  EXPECT_EQ(1000000, FragmentAt(flow_thread, 0)
                         .PaintProperties()
                         ->FragmentClip()
                         ->UnsnappedClipRect()
                         .Rect()
                         .MaxX());
  EXPECT_EQ(-999750, FragmentAt(flow_thread, 1)
                         .PaintProperties()
                         ->FragmentClip()
                         ->UnsnappedClipRect()
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

  blended_element->setAttribute(html_names::kStyleAttr,
                                "mix-blend-mode: lighten;");
  UpdateAllLifecyclePhasesForTest();

  props = blended_element->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_TRUE(props->Effect());
  EXPECT_EQ(props->Effect()->BlendMode(), SkBlendMode::kLighten);
}

TEST_P(PaintPropertyTreeUpdateTest, EnsureSnapContainerData) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html {
      scroll-snap-type: both proximity;
    }
    body {
      overflow: scroll;
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
  // Manually set the visual viewport size because the testing client does not
  // do this. The size needs to be updated because otherwise the
  // RootFrameViewport's maximum scroll offset would be negative and trigger a
  // DCHECK.
  GetDocument().GetPage()->GetVisualViewport().SetSize(IntSize(300, 300));
  UpdateAllLifecyclePhasesForTest();

  auto doc_snap_container_data = DocScroll()->GetSnapContainerData();
  ASSERT_TRUE(doc_snap_container_data);
  EXPECT_EQ(doc_snap_container_data->scroll_snap_type().axis,
            cc::SnapAxis::kBoth);
  EXPECT_EQ(doc_snap_container_data->scroll_snap_type().strictness,
            cc::SnapStrictness::kProximity);
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
  descendant->setAttribute(html_names::kStyleAttr, "position: relative");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(clip_properties->OverflowClip(),
            effect_properties->Effect()->OutputClip());

  descendant->setAttribute(html_names::kStyleAttr, "position: absolute");
  UpdateAllLifecyclePhasesForTest();
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
                *rect_properties->Transform(),
                *svg2_properties->PaintOffsetTranslation())
                .Matrix());

  // Change filter which forward references rect, and insert a transform
  // node above rect's transform.
  GetDocument().getElementById("filter")->setAttribute("width", "20");
  GetDocument().getElementById("svg2")->setAttribute("transform",
                                                     "translate(2)");
  UpdateAllLifecyclePhasesForTest();

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
                *rect_properties->Transform(),
                *svg2_properties->PaintOffsetTranslation())
                .Matrix());
}

TEST_P(PaintPropertyTreeUpdateTest, OverflowClipUpdateForImage) {
  // This test verifies clip nodes are correctly updated in response to
  // content box mutation.
  SetBodyInnerHTML(R"HTML(
    <style>
    img {
      box-sizing: border-box;
      width: 8px;
      height: 8px;
    }
    </style>
    <!-- An image of 10x10 white pixels. -->
    <img id="target" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAA
        AAKCAIAAAACUFjqAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4gcVABQvx8CBmA
        AAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAAFUlEQVQY02P
        8//8/A27AxIAXjFRpAKXjAxH/0Dm5AAAAAElFTkSuQmCC">
  )HTML");

  auto* target = GetDocument().getElementById("target");
  const auto* properties = PaintPropertiesForElement("target");
  // We don't need paint properties for object-fit: fill because the content
  // never overflows.
  EXPECT_EQ(nullptr, properties);

  target->setAttribute(html_names::kStyleAttr, "object-fit: cover");
  UpdateAllLifecyclePhasesForTest();
  properties = PaintPropertiesForElement("target");
  // We don't need paint properties because image painter always clip to the
  // content box.
  EXPECT_EQ(nullptr, properties);

  target->setAttribute(html_names::kStyleAttr, "object-fit: none");
  UpdateAllLifecyclePhasesForTest();
  properties = PaintPropertiesForElement("target");
  // Ditto.
  EXPECT_EQ(nullptr, properties);

  // We need overflow clip when there is border radius.
  target->setAttribute(html_names::kStyleAttr,
                       "object-fit: none; border-radius: 2px");
  UpdateAllLifecyclePhasesForTest();
  properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->OverflowClip());
  FloatSize corner(2, 2);
  FloatRoundedRect::Radii radii(corner, corner, corner, corner);
  EXPECT_EQ(FloatRoundedRect(FloatRect(8, 8, 8, 8), radii),
            properties->OverflowClip()->UnsnappedClipRect());

  // We should update clip rect on border radius change.
  target->setAttribute(html_names::kStyleAttr,
                       "object-fit: none; border-radius: 3px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("target"));
  ASSERT_TRUE(properties->OverflowClip());
  radii.Expand(1);
  EXPECT_EQ(FloatRoundedRect(FloatRect(8, 8, 8, 8), radii),
            properties->OverflowClip()->UnsnappedClipRect());

  // We should update clip rect on padding change.
  target->setAttribute(
      html_names::kStyleAttr,
      "object-fit: none; border-radius: 3px; padding: 1px 2px 3px 4px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("target"));
  ASSERT_TRUE(properties->OverflowClip());
  // The rounded clip rect is the intersection of the rounded inner border
  // rect and the content box rect.
  EXPECT_EQ(
      FloatRoundedRect(FloatRect(12, 9, 2, 4),
                       FloatRoundedRect::Radii(FloatSize(0, 2), FloatSize(1, 2),
                                               FloatSize(), FloatSize(1, 0))),
      properties->OverflowClip()->UnsnappedClipRect());
}

TEST_P(PaintPropertyTreeUpdateTest, OverflowClipUpdateForVideo) {
  // This test verifies clip nodes are correctly updated in response to
  // content box mutation.
  SetBodyInnerHTML(R"HTML(
    <style>
    video {
      box-sizing: border-box;
      width: 8px;
      height: 8px;
    }
    </style>
    <video id="target"></video>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  const auto* properties = PaintPropertiesForElement("target");
  // We always create overflow clip for video regardless of object-fit.
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->OverflowClip());
  EXPECT_EQ(FloatRoundedRect(8, 8, 8, 8),
            properties->OverflowClip()->UnsnappedClipRect());

  target->setAttribute(html_names::kStyleAttr, "object-fit: cover");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("target"));
  ASSERT_TRUE(properties->OverflowClip());
  EXPECT_EQ(FloatRoundedRect(8, 8, 8, 8),
            properties->OverflowClip()->UnsnappedClipRect());

  // We need OverflowClip for object-fit: cover, too.
  target->setAttribute(html_names::kStyleAttr, "object-fit: none");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("target"));
  ASSERT_TRUE(properties->OverflowClip());
  EXPECT_EQ(FloatRoundedRect(8, 8, 8, 8),
            properties->OverflowClip()->UnsnappedClipRect());

  // We should update clip rect on padding change.
  target->setAttribute(html_names::kStyleAttr,
                       "object-fit: none; padding: 1px 2px 3px 4px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("target"));
  ASSERT_TRUE(properties->OverflowClip());
  EXPECT_EQ(FloatRoundedRect(12, 9, 2, 4),
            properties->OverflowClip()->UnsnappedClipRect());
}

TEST_P(PaintPropertyTreeUpdateTest, ChangingClipPath) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      #content {
        height: 500px;
        width: 200px;
        overflow: scroll;
      }
      .aclippath { clip-path: circle(115px at 20px 20px); }
      .bclippath { clip-path: circle(135px at 22px 20px); }
    </style>
    <div id="content"></div>
  )HTML");
  auto* content = GetDocument().getElementById("content");
  content->setAttribute(html_names::kClassAttr, "aclippath");
  UpdateAllLifecyclePhasesForTest();

  content->setAttribute(html_names::kClassAttr, "bclippath");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crash.

  content->removeAttribute(html_names::kClassAttr);
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crash.
}

TEST_P(PaintPropertyTreeUpdateTest, SubpixelAccumulationAcrossIsolation) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="parent" style="margin-left: 10.25px">
      <div id="isolation" style="contain: paint">
        <div id="child"><div>
      </div>
    </div>
  )HTML");
  auto* parent_element = GetDocument().getElementById("parent");
  auto* parent = parent_element->GetLayoutObject();
  auto* isolation_properties = PaintPropertiesForElement("isolation");
  auto* child = GetLayoutObjectByElementId("child");
  EXPECT_EQ(PhysicalOffset(LayoutUnit(10.25), LayoutUnit()),
            parent->FirstFragment().PaintOffset());
  EXPECT_EQ(FloatSize(10, 0),
            isolation_properties->PaintOffsetTranslation()->Translation2D());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(PhysicalOffset(), child->FirstFragment().PaintOffset());
  } else {
    EXPECT_EQ(PhysicalOffset(LayoutUnit(0.25), LayoutUnit()),
              child->FirstFragment().PaintOffset());
  }

  parent_element->setAttribute(html_names::kStyleAttr, "margin-left: 12.75px");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(PhysicalOffset(LayoutUnit(12.75), LayoutUnit()),
            parent->FirstFragment().PaintOffset());
  EXPECT_EQ(FloatSize(13, 0),
            isolation_properties->PaintOffsetTranslation()->Translation2D());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(PhysicalOffset(), child->FirstFragment().PaintOffset());
  } else {
    EXPECT_EQ(PhysicalOffset(LayoutUnit(-0.25), LayoutUnit()),
              child->FirstFragment().PaintOffset());
  }
}

TEST_P(PaintPropertyTreeUpdateTest, ChangeDuringAnimation) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes animation {
          0% { opacity: 0.3; }
          100% { opacity: 0.4; }
        }
        #target {
          animation-name: animation;
          animation-duration: 1s;
          width: 100px;
          height: 100px;
        }
      </style>
      <div id='target'></div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  auto style = ComputedStyle::Clone(target->StyleRef());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  // Simulates starting a composite animation.
  style->SetHasCurrentTransformAnimation(true);
  style->SetIsRunningTransformAnimationOnCompositor(true);
  target->SetStyle(std::move(style));
  EXPECT_TRUE(target->NeedsPaintPropertyUpdate());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  const auto* transform_node =
      target->FirstFragment().PaintProperties()->Transform();
  ASSERT_TRUE(transform_node);
  EXPECT_TRUE(transform_node->HasActiveTransformAnimation());
  EXPECT_EQ(TransformationMatrix(), transform_node->Matrix());
  EXPECT_EQ(FloatPoint3D(50, 50, 0), transform_node->Origin());
  // Change of animation status should update PaintArtifactCompositor.
  auto* paint_artifact_compositor =
      GetDocument().View()->GetPaintArtifactCompositor();
  EXPECT_TRUE(paint_artifact_compositor->NeedsUpdate());
  // PaintArtifactCompositor can't clear the NeedsUpdate flag by itself when
  // there is no cc::LayerTreeHost.
  paint_artifact_compositor->ClearNeedsUpdateForTesting();

  // Simulates changing transform and transform-origin during an animation.
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style = ComputedStyle::Clone(target->StyleRef());
  TransformOperations transform;
  transform.Operations().push_back(
      RotateTransformOperation::Create(10, TransformOperation::kRotate));
  style->SetTransform(transform);
  style->SetTransformOrigin(TransformOrigin(Length(70, Length::kFixed),
                                            Length(30, Length::kFixed), 0));
  target->SetStyle(std::move(style));
  EXPECT_TRUE(target->NeedsPaintPropertyUpdate());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  ASSERT_EQ(transform_node,
            target->FirstFragment().PaintProperties()->Transform());
  EXPECT_TRUE(transform_node->HasActiveTransformAnimation());
  EXPECT_EQ(TransformationMatrix().Rotate(10), transform_node->Matrix());
  EXPECT_EQ(FloatPoint3D(70, 30, 0), transform_node->Origin());
  EXPECT_TRUE(transform_node->BackfaceVisibilitySameAsParent());
  // Changing only transform or transform-origin values during a composited
  // animation should not schedule a PaintArtifactCompositor update.
  EXPECT_FALSE(paint_artifact_compositor->NeedsUpdate());

  // Simulates changing backface visibility during animation.
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  style = ComputedStyle::Clone(target->StyleRef());
  style->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  target->SetStyle(std::move(style));
  EXPECT_TRUE(target->NeedsPaintPropertyUpdate());
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  ASSERT_EQ(transform_node,
            target->FirstFragment().PaintProperties()->Transform());
  EXPECT_TRUE(transform_node->HasActiveTransformAnimation());
  EXPECT_EQ(TransformationMatrix().Rotate(10), transform_node->Matrix());
  EXPECT_EQ(FloatPoint3D(70, 30, 0), transform_node->Origin());
  EXPECT_FALSE(transform_node->BackfaceVisibilitySameAsParent());
  // Only transform and transform-origin value changes during composited
  // animation should not schedule PaintArtifactCompositor update. Backface
  // visibility changes should schedule an update.
  EXPECT_TRUE(paint_artifact_compositor->NeedsUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, BackfaceVisibilityInvalidatesProperties) {
  SetBodyInnerHTML("<span id='span'>a</span>");

  auto* span = GetDocument().getElementById("span");
  span->setAttribute(html_names::kStyleAttr, "backface-visibility: hidden;");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(span->GetLayoutObject()->NeedsPaintPropertyUpdate());
}

TEST_P(PaintPropertyTreeUpdateTest, FixedPositionCompositing) {
  SetBodyInnerHTML(R"HTML(
    <div id="space" style="height: 200px"></div>
    <div id="fixed" style="position: fixed; top: 50px; left: 60px">Fixed</div>
  )HTML");

  EXPECT_FALSE(PaintPropertiesForElement("fixed"));

  auto* space = GetDocument().getElementById("space");
  space->setAttribute(html_names::kStyleAttr, "height: 2000px");
  UpdateAllLifecyclePhasesForTest();
  auto* properties = PaintPropertiesForElement("fixed");
  ASSERT_TRUE(properties);
  auto* paint_offset_translation = properties->PaintOffsetTranslation();
  ASSERT_TRUE(paint_offset_translation);
  EXPECT_EQ(FloatSize(60, 50), paint_offset_translation->Translation2D());
  EXPECT_TRUE(paint_offset_translation->HasDirectCompositingReasons());
  EXPECT_FALSE(properties->Transform());

  space->setAttribute(html_names::kStyleAttr, "height: 100px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(PaintPropertiesForElement("fixed"));
}

TEST_P(PaintPropertyTreeUpdateTest, InlineFilterReferenceBoxChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="spacer" style="display: inline-block; height: 20px"></div>
    <br>
    <span id="span" style="filter: blur(1px); font-size: 20px">SPAN</span>
  )HTML");

  const auto* properties = PaintPropertiesForElement("span");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Filter());
  EXPECT_EQ(FloatPoint(0, 20),
            properties->Filter()->Filter().ReferenceBox().Location());

  GetDocument().getElementById("spacer")->setAttribute(
      html_names::kStyleAttr, "display: inline-block; height: 100px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("span"));
  EXPECT_EQ(FloatPoint(0, 100),
            properties->Filter()->Filter().ReferenceBox().Location());
}

TEST_P(PaintPropertyTreeUpdateTest, StartSVGAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>line {transition: transform 1s; transform: translateY(1px)}</style>
    <svg width="200" height="200" stroke="black">
      <line id="line" x1="0" y1="0" x2="150" y2="50">
    </svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("line");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Transform());
  EXPECT_FALSE(properties->Transform()->HasDirectCompositingReasons());

  GetDocument().getElementById("line")->setAttribute(
      html_names::kStyleAttr, "transform: translateY(100px)");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(properties, PaintPropertiesForElement("line"));
  EXPECT_TRUE(properties->Transform()->HasDirectCompositingReasons());
}

}  // namespace blink
