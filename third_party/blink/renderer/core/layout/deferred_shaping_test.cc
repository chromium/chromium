// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/deferred_shaping_controller.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class DeferredShapingTest : public RenderingTest {
 protected:
  bool IsDefer(const char* id_value) const {
    const auto* layout_object = GetLayoutObjectByElementId(id_value);
    return layout_object && layout_object->IsShapingDeferred();
  }

  void ScrollAndLayout(double new_scroll_top) {
    GetDocument().scrollingElement()->setScrollTop(new_scroll_top);
    UpdateAllLifecyclePhasesForTest();
  }

 private:
  ScopedLayoutNGForTest enablee_layout_ng_{true};
  ScopedDeferredShapingForTest enable_deferred_shapign_{true};
};

TEST_F(DeferredShapingTest, Basic) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, NoViewportMargin) {
  // The box starting around y=600 (== viewport height) is deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:600px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, AlreadyAuto) {
  // If the element has content-visibility:auto, it never be deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target" style="content-visibility:auto">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, AlreadyHidden) {
  // If the element has content-visibility:hidden, it never be deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target" style="content-visibility:hidden">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, DynamicAuto) {
  // If a deferred element gets content-visibility:auto, it stops deferring.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("target")->setAttribute("style", "content-visibility:auto");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, DynamicHidden) {
  // If a deferred element gets content-visibility:hidden, it stops deferring.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("target")->setAttribute("style", "content-visibility:hidden");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));

  GetElementById("target")->setAttribute("style", "content-visibility:visible");
  // A change of content-visibility property triggers a full layout, and the
  // target box is determined as "deferred" again.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, DynamicPropertyChange) {
  // If a property of a deferred element is changed, it keeps deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("target")->setAttribute("style", "width: 10em;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, ListMarkerCrash) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<ul>
<li id="target">IFC</li>
</ul>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  // Re-layout the target while deferred.
  GetElementById("target")->setTextContent("foobar");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crash.
}

TEST_F(DeferredShapingTest, FragmentItemCache) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<p id="target" style="font-family:Times; width:100px">
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  auto* target_box = GetLayoutBoxByElementId("target");
  const LayoutUnit deferred_item_width =
      (*target_box->PhysicalFragments().begin())
          .Items()
          ->Items()[0]
          .Size()
          .width;

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_NE(deferred_item_width, (*target_box->PhysicalFragments().begin())
                                     .Items()
                                     ->Items()[0]
                                     .Size()
                                     .width);
}

TEST_F(DeferredShapingTest, FragmentItemCacheWithMinMax) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div style="display:flex">
<div style="max-width: 100px; align-self:center; flex:1 1 auto">
<p id="target-p" style="font-family:Times;max-width: 100px;">
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
MMM MMMMM MMMMM MMM MMMMM MMMM MMM MMMM MMM.
</p></div></div>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target-p"));
  auto* target_box = GetLayoutBoxByElementId("target-p");
  const LayoutUnit deferred_item_width =
      (*target_box->PhysicalFragments().begin())
          .Items()
          ->Items()[0]
          .Size()
          .width;

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target-p"));
  EXPECT_NE(deferred_item_width, (*target_box->PhysicalFragments().begin())
                                     .Items()
                                     ->Items()[0]
                                     .Size()
                                     .width);
}

// crbug.com/1327891
TEST_F(DeferredShapingTest, FragmentAssociationAfterUnlock) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  auto* box = GetLayoutBoxByElementId("target");
  auto* fragment = box->GetPhysicalFragment(0);
  EXPECT_EQ(box, fragment->GetLayoutObject());

  ScrollAndLayout(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_EQ(nullptr, fragment->GetLayoutObject());
}

TEST_F(DeferredShapingTest, UpdateTextInDeferred) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<p id="target">IFC</p>
</ul>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  DeferredShapingController::From(GetDocument())->DisallowDeferredShaping();

  // Re-layout the target while it was deferred but deferred shaping is
  // disabled. We had an inconsistent state issue.
  GetElementById("target")->setTextContent("foobar");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, UnlockNestedDeferred) {
  // 'M' is used here because it is typically wider than ' '.
  SetBodyInnerHTML(
      uR"HTML(<div  style="font-family:Times; font-size:50px;">
<p>IFC<ruby>b<rt id="ref2">MMMMMMM MMMMMMM MMMMMMM</rt></ruby></p>
<div style="height:1800px"></div>
<p id="target">IFC<ruby>b<rt id="target2">MMMMMMM MMMMMMM MMMMMMM</rt></ruby>
</p></div>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsDefer("target2"));

  ScrollAndLayout(1800);
  // Nested deferred IFCs are re-shaped together.
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_FALSE(IsDefer("target2"));
  EXPECT_EQ(GetElementById("ref2")->clientWidth(),
            GetElementById("target2")->clientWidth());
}

TEST_F(DeferredShapingTest, UnlockOnSwitchingToFlex) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="target">IFC</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("target")->setAttribute("style", "display:flex");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, UnlockOnSwitchingToAnotherBlockFlow) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="target">IFC</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("target")->setAttribute("style", "display:inline-block");
  UpdateAllLifecyclePhasesForTest();
  // Switching from 'block' to 'inline-block' unlocks the element
  // then locks the element again.
  EXPECT_TRUE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, UnlockOnDetach) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<div id="container"><p id="target">IFC</p></div>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("container")->setAttribute("style", "display:none");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, UnlockOnSwithcingToBfc) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="target">IFC</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("target")->appendChild(
      GetDocument().CreateRawElement(html_names::kDivTag));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
}

// crbug.com/1324458
TEST_F(DeferredShapingTest, UnlockOnSwithcingToBfcByChildPositionChange) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<li id="target">\n<div id="abs" style="position:absolute"></div></li>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetElementById("abs")->setAttribute("style", "position:static");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
}

// crbug.com/1335731
TEST_F(DeferredShapingTest, KeepDeferredAfterTextChange) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="target">ifc<p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  auto* target = GetElementById("target");
  target->appendChild(GetDocument().createTextNode("ifc2 "));
  target->appendChild(GetDocument().createTextNode("ifc3 "));
  UpdateAllLifecyclePhasesForTest();
  target->normalize();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, ScrollIntoView) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<div><p id="prior">IFC</p></div>
<div style="height:3600px"></div>
<p id="ancestor">IFC<span style="display:inline-block" id="target"></sapn></p>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("prior"));
  EXPECT_TRUE(IsDefer("ancestor"));

  GetElementById("target")->scrollIntoView();
  EXPECT_FALSE(IsDefer("prior"));
  EXPECT_FALSE(IsDefer("ancestor"));
}

TEST_F(DeferredShapingTest, ElementGeometry) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="ancestor">IFC
<span style="display:inline-block" id="previous">MMMM MMMM MMMM</sapn>
<span id="target">inline</span>
</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("previous"));
  EXPECT_TRUE(IsDefer("ancestor"));

  GetElementById("target")->getBoundingClientRect();
  EXPECT_FALSE(IsDefer("previous"));
  EXPECT_FALSE(IsDefer("ancestor"));
}

TEST_F(DeferredShapingTest, ElementGeometryContainingDeferred) {
  SetBodyInnerHTML(R"HTML(<div style="display:inline-block" id="reference">
<div style="display:inline-block">MMMM MMMM MMMM</div></div>
<div style="height:1800px"></div>
<div style="display:inline-block" id="target">
<div style="display:inline-block" id="target-child">MMMM MMMM MMMM</div></div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target-child"));

  DOMRect& reference = *GetElementById("reference")->getBoundingClientRect();
  DOMRect& target = *GetElementById("target")->getBoundingClientRect();
  EXPECT_EQ(reference.width(), target.width());
  EXPECT_EQ(reference.height(), target.height());
  EXPECT_FALSE(IsDefer("target-child"));
}

TEST_F(DeferredShapingTest, ElementGeometryAllReshape) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="previous">Previous IFC</p>
<p id="ancestor">IFC
<span id="inline_target">inline</span>
</p>
<div id="block_target"><p id="inner">IFC</p></div>
<div id="abs_block" style="position:absolute; right:10px; bottom:42px"></div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("previous"));
  EXPECT_TRUE(IsDefer("ancestor"));
  EXPECT_TRUE(IsDefer("inner"));

  To<HTMLElement>(GetElementById("inline_target"))->offsetWidthForBinding();
  EXPECT_FALSE(IsDefer("previous"));
  EXPECT_FALSE(IsDefer("ancestor"));

  To<HTMLElement>(GetElementById("block_target"))->offsetHeightForBinding();
  EXPECT_FALSE(IsDefer("previous"));
  EXPECT_FALSE(IsDefer("inner"));

  To<HTMLElement>(GetElementById("abs_block"))->getBoundingClientRect();
  EXPECT_FALSE(IsDefer("previous"));
  EXPECT_FALSE(IsDefer("abs_block"));
}

TEST_F(DeferredShapingTest, RangeGetClientRects) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="ancestor">IFC
<span style="display:inline-block" id="previous">MMMM MMMM MMMM</sapn>
<span id="target">inline</span>
</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("previous"));
  EXPECT_TRUE(IsDefer("ancestor"));

  Element* target = GetElementById("target");
  Range* range = GetDocument().createRange();
  range->setStart(target, 0);
  range->setEnd(target, 1);
  // getClientRects() should re-shape all elements.
  range->getClientRects();

  EXPECT_FALSE(IsDefer("previous"));
  EXPECT_FALSE(IsDefer("ancestor"));
}

TEST_F(DeferredShapingTest, RangeGetBoundingClientRect) {
  SetBodyInnerHTML(R"HTML(<div style="height:1800px"></div>
<p id="ancestor">IFC
<span style="display:inline-block" id="previous">MMMM MMMM MMMM</sapn>
<span id="target">inline</span>
</p>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("previous"));
  EXPECT_TRUE(IsDefer("ancestor"));

  Element* target = GetElementById("target");
  Range* range = GetDocument().createRange();
  range->setStart(target, 0);
  range->setEnd(target, 1);
  // getBoundingClientRect() should re-shape all elements.
  range->getBoundingClientRect();

  EXPECT_FALSE(IsDefer("previous"));
  EXPECT_FALSE(IsDefer("ancestor"));
}

TEST_F(DeferredShapingTest, NonLayoutNGBlockFlow) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<table><caption id="target">IFC</caption></table>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // LayoutNGTableCaption, which is not a subclass of LayoutNGBlockFlow,
  // should support IsShapingDeferred().
  EXPECT_TRUE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, ShapeResultCrash) {
  StringBuilder builder;
  builder.ReserveCapacity(1000);
  builder.Append(R"HTML(
<div style="height:1800px"></div><p>)HTML");
  for (unsigned i = 0; i < HarfBuzzRunGlyphData::kMaxCharacterIndex + 10; ++i)
    builder.Append('M');
  builder.Append("</p>");
  SetBodyInnerHTML(builder.ToString());
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

TEST_F(DeferredShapingTest, InnerText) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px">Not-deferred</div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_EQ("Not-deferred\nIFC", GetDocument().body()->innerText());
}

TEST_F(DeferredShapingTest, PositionForPoint) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px">Not-deferred</div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  auto& target = To<Element>(*GetElementById("target"));
  Node* text = target.firstChild();
  gfx::Rect text_rect =
      MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 3)
          ->BoundingBox();
  auto position = target.GetLayoutBox()->PositionForPoint(
      {text_rect.width(), text_rect.height()});
  EXPECT_EQ(3, position.GetPosition().OffsetInContainerNode());
}

TEST_F(DeferredShapingTest, DeferThenPrint) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  // Shaping-deferred elements are unlocked by printing.
  ScopedPrintContext print_context(&GetFrame());
  print_context->BeginPrintMode(800, 600);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, NoDeferDuringPrint) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");

  // Printing layout produces no shaping-deferred elements.
  ScopedPrintContext print_context(&GetFrame());
  print_context->BeginPrintMode(800, 600);
  EXPECT_FALSE(IsDefer("target"));
}

TEST_F(DeferredShapingTest, NoDeferForAutoSizing) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    @media (max-height: 200px) {
      #target { display: inline; }
    }
    </style>
    <div style="height:1800px"></div>
    <div id="target">IFC</div>)HTML",
                                     ASSERT_NO_EXCEPTION);

  GetFrame().View()->EnableAutoSizeMode({100, 100}, {1920, 4000});
  UpdateAllLifecyclePhasesForTest();
  // Pass if no DCHECK failures.
}

TEST_F(DeferredShapingTest, ScrollIntoViewInInactiveDocument) {
  ScopedNullExecutionContext execution_context;
  Document* doc =
      Document::CreateForTest(execution_context.GetExecutionContext());
  Node* root = doc->appendChild(doc->CreateRawElement(html_names::kHTMLTag));
  To<Element>(root)->scrollIntoView();
  // PASS if no crash.
}

TEST_F(DeferredShapingTest, ResizeFrame) {
  SetBodyInnerHTML(R"HTML(
      <div style="height:600px"></div>
      <div id="target" style="height:800px">IFC</div>
      <div id="target2">IFC2</div>
      )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));

  GetFrame().View()->SetLayoutSizeFixedToFrameSize(false);
  GetFrame().View()->SetLayoutSize({800, 1200});
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_TRUE(IsDefer("target2"));
}

TEST_F(DeferredShapingTest, OnFocus) {
  SetBodyInnerHTML(R"HTML(
      <div id="target1" tabindex="1">IFC</div>
      <div style="height:600px"></div>
      <div id="target2" tabindex="1">IFC</div>
      )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target1"));
  EXPECT_TRUE(IsDefer("target2"));

  GetElementById("target1")->Focus();
  EXPECT_TRUE(IsDefer("target2"));

  GetElementById("target2")->Focus();
  EXPECT_FALSE(IsDefer("target2"));
}

}  // namespace blink
