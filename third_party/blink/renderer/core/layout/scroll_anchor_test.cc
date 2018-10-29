// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/scroll_anchor.h"

#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

class ScrollAnchorTest : public testing::WithParamInterface<bool>,
                         private ScopedLayoutNGForTest,
                         public RenderingTest {
 public:
  ScrollAnchorTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  void Update() {
    // TODO(skobes): Use SimTest instead of RenderingTest and move into
    // Source/web?
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  VisualViewport& GetVisualViewport() {
    return GetDocument().View()->GetPage()->GetVisualViewport();
  }

  ScrollableArea* ScrollerForElement(Element* element) {
    return ToLayoutBox(element->GetLayoutObject())->GetScrollableArea();
  }

  ScrollAnchor& GetScrollAnchor(ScrollableArea* scroller) {
    DCHECK(scroller->IsLocalFrameView() ||
           scroller->IsPaintLayerScrollableArea());
    return *(scroller->GetScrollAnchor());
  }

  void SetHeight(Element* element, int height) {
    element->setAttribute(HTMLNames::styleAttr,
                          AtomicString(String::Format("height: %dpx", height)));
    Update();
  }

  void ScrollLayoutViewport(ScrollOffset delta) {
    Element* scrolling_element = GetDocument().scrollingElement();
    if (delta.Width())
      scrolling_element->setScrollLeft(scrolling_element->scrollLeft() +
                                       delta.Width());
    if (delta.Height())
      scrolling_element->setScrollTop(scrolling_element->scrollTop() +
                                      delta.Height());
  }

  void ValidateSerializedAnchor(const String& expected_selector,
                                const LayoutPoint& expected_offset) {
    SerializedAnchor serialized =
        GetScrollAnchor(LayoutViewport()).GetSerializedAnchor();
    EXPECT_TRUE(serialized.IsValid());
    EXPECT_EQ(serialized.selector, expected_selector);
    EXPECT_EQ(serialized.relative_offset, expected_offset);

    StaticElementList* ele_list =
        GetDocument().QuerySelectorAll(AtomicString(serialized.selector));
    EXPECT_EQ(ele_list->length(), 1u);
  }
};

INSTANTIATE_TEST_CASE_P(All, ScrollAnchorTest, testing::Bool());

// TODO(ymalik): Currently, this should be the first test in the file to avoid
// failure when running with other tests. Dig into this more and fix.
TEST_P(ScrollAnchorTest, UMAMetricUpdated) {
  HistogramTester histogram_tester;
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } div { height: 100px } </style>
    <div id='block1'>abc</div>
    <div id='block2'>def</div>
    <script></script>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();

  // Scroll position not adjusted, metric not updated.
  ScrollLayoutViewport(ScrollOffset(0, 150));
  histogram_tester.ExpectTotalCount("Layout.ScrollAnchor.AdjustedScrollOffset",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Layout.ScrollAnchor.TimeToComputeAnchorNodeSelector", 0);

  // Height changed, verify metric updated once.
  SetHeight(GetDocument().getElementById("block1"), 200);
  histogram_tester.ExpectUniqueSample(
      "Layout.ScrollAnchor.AdjustedScrollOffset", 1, 1);

  EXPECT_EQ(250, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().getElementById("block2")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  GetScrollAnchor(viewport).GetSerializedAnchor();
  histogram_tester.ExpectTotalCount(
      "Layout.ScrollAnchor.TimeToComputeAnchorNodeSelector", 1);
  // 7 == "#block2".length()
  histogram_tester.ExpectUniqueSample(
      "Layout.ScrollAnchor.SerializedAnchorSelectorLength", 7, 1);

  // Clear the current anchor so that we can test restoration histograms.
  // Restoration only proceeds if there isn't an existing anchor.
  GetScrollAnchor(viewport).Clear();

  SerializedAnchor bad_anchor("##foobar", LayoutPoint(0, 0));
  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).RestoreAnchor(bad_anchor));
  histogram_tester.ExpectBucketCount("Layout.ScrollAnchor.RestorationStatus",
                                     ScrollAnchor::kFailedBadSelector, 1);

  SerializedAnchor bad_anchor2("#bl", LayoutPoint(0, 0));
  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).RestoreAnchor(bad_anchor2));
  histogram_tester.ExpectBucketCount("Layout.ScrollAnchor.RestorationStatus",
                                     ScrollAnchor::kFailedNoMatches, 1);

  SerializedAnchor bad_anchor3("script", LayoutPoint(0, -1000));
  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).RestoreAnchor(bad_anchor3));
  histogram_tester.ExpectBucketCount("Layout.ScrollAnchor.RestorationStatus",
                                     ScrollAnchor::kFailedNoValidMatches, 1);

  SerializedAnchor serialized_anchor("#block1", LayoutPoint(0, 0));
  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));

  histogram_tester.ExpectTotalCount("Layout.ScrollAnchor.TimeToRestoreAnchor",
                                    4);
  histogram_tester.ExpectBucketCount("Layout.ScrollAnchor.RestorationStatus",
                                     ScrollAnchor::kSuccess, 1);
}

// TODO(skobes): Convert this to web-platform-tests when visual viewport API is
// launched (http://crbug.com/635031).
TEST_P(ScrollAnchorTest, VisualViewportAnchors) {
  SetBodyInnerHTML(R"HTML(
    <style>
        * { font-size: 1.2em; font-family: sans-serif; }
        div { height: 100px; width: 20px; background-color: pink; }
    </style>
    <div id='div'></div>
    <div id='text'><b>This is a scroll anchoring test</div>
  )HTML");

  ScrollableArea* l_viewport = LayoutViewport();
  VisualViewport& v_viewport = GetVisualViewport();

  v_viewport.SetScale(2.0);

  // No anchor at origin (0,0).
  EXPECT_EQ(nullptr, GetScrollAnchor(l_viewport).AnchorObject());

  // Scroll the visual viewport to bring #text to the top.
  int top =
      GetDocument().getElementById("text")->getBoundingClientRect()->top();
  v_viewport.SetLocation(FloatPoint(0, top));

  SetHeight(GetDocument().getElementById("div"), 10);
  EXPECT_EQ(GetDocument().getElementById("text")->GetLayoutObject(),
            GetScrollAnchor(l_viewport).AnchorObject());
  EXPECT_EQ(top - 90, v_viewport.ScrollOffsetInt().Height());

  SetHeight(GetDocument().getElementById("div"), 100);
  EXPECT_EQ(GetDocument().getElementById("text")->GetLayoutObject(),
            GetScrollAnchor(l_viewport).AnchorObject());
  EXPECT_EQ(top, v_viewport.ScrollOffsetInt().Height());

  // Scrolling the visual viewport should clear the anchor.
  v_viewport.SetLocation(FloatPoint(0, 0));
  EXPECT_EQ(nullptr, GetScrollAnchor(l_viewport).AnchorObject());
}

// Test that a non-anchoring scroll on scroller clears scroll anchors for all
// parent scrollers.
TEST_P(ScrollAnchorTest, ClearScrollAnchorsOnAncestors) {
  SetBodyInnerHTML(R"HTML(
    <style>
        body { height: 1000px } div { height: 200px }
        #scroller { height: 100px; width: 200px; overflow: scroll; }
    </style>
    <div id='changer'>abc</div>
    <div id='anchor'>def</div>
    <div id='scroller'><div></div></div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();

  ScrollLayoutViewport(ScrollOffset(0, 250));
  SetHeight(GetDocument().getElementById("changer"), 300);

  EXPECT_EQ(350, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(GetDocument().getElementById("anchor")->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // Scrolling the nested scroller should clear the anchor on the main frame.
  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().getElementById("scroller"));
  scroller->ScrollBy(ScrollOffset(0, 100), kUserScroll);
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, AncestorClearingWithSiblingReference) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .scroller {
      overflow: scroll;
      width: 400px;
      height: 400px;
    }
    .space {
      width: 100px;
      height: 600px;
    }
    </style>
    <div id='s1' class='scroller'>
      <div id='anchor' class='space'></div>
    </div>
    <div id='s2' class='scroller'>
      <div class='space'></div>
    </div>
  )HTML");
  Element* s1 = GetDocument().getElementById("s1");
  Element* s2 = GetDocument().getElementById("s2");
  Element* anchor = GetDocument().getElementById("anchor");

  // Set non-zero scroll offsets for #s1 and #document
  s1->setScrollTop(100);
  ScrollLayoutViewport(ScrollOffset(0, 100));

  // Invalidate layout.
  SetHeight(anchor, 500);

  // This forces layout, during which both #s1 and #document will anchor to
  // #anchor. Then the scroll clears #s2 and #document.  Since #anchor is still
  // referenced by #s1, its IsScrollAnchorObject bit must remain set.
  s2->setScrollTop(100);

  // This should clear #s1.  If #anchor had its bit cleared already we would
  // crash in update().
  s1->RemoveChild(anchor);
  Update();
}

TEST_P(ScrollAnchorTest, FractionalOffsetsAreRoundedBeforeComparing) {
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } </style>
    <div id='block1' style='height: 50.4px'>abc</div>
    <div id='block2' style='height: 100px'>def</div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 100));

  GetDocument().getElementById("block1")->setAttribute(HTMLNames::styleAttr,
                                                       "height: 50.6px");
  Update();

  EXPECT_EQ(101, viewport->ScrollOffsetInt().Height());
}

TEST_P(ScrollAnchorTest, AvoidStickyAnchorWhichMovesWithScroll) {
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } </style>
    <div id='block1' style='height: 50px'>abc</div>
    <div id='block2' style='height: 100px; position: sticky; top: 0;'>
        def</div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 60));

  GetDocument().getElementById("block1")->setAttribute(HTMLNames::styleAttr,
                                                       "height: 100px");
  Update();

  EXPECT_EQ(60, viewport->ScrollOffsetInt().Height());
}

TEST_P(ScrollAnchorTest, AnchorWithLayerInScrollingDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>
        #scroller { overflow: scroll; width: 500px; height: 400px; }
        div { height: 100px }
        #block2 { overflow: hidden }
        #space { height: 1000px; }
    </style>
    <div id='scroller'><div id='space'>
    <div id='block1'>abc</div>
    <div id='block2'>def</div>
    </div></div>
  )HTML");

  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().getElementById("scroller"));
  Element* block1 = GetDocument().getElementById("block1");
  Element* block2 = GetDocument().getElementById("block2");

  scroller->ScrollBy(ScrollOffset(0, 150), kUserScroll);

  // In this layout pass we will anchor to #block2 which has its own PaintLayer.
  SetHeight(block1, 200);
  EXPECT_EQ(250, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(block2->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());

  // Test that the anchor object can be destroyed without affecting the scroll
  // position.
  block2->remove();
  Update();
  EXPECT_EQ(250, scroller->ScrollOffsetInt().Height());
}

// Verify that a nested scroller with a div that has its own PaintLayer can be
// removed without causing a crash. This test passes if it doesn't crash.
TEST_P(ScrollAnchorTest, RemoveScrollerWithLayerInScrollingDiv) {
  SetBodyInnerHTML(R"HTML(
    <style>
        body { height: 2000px }
        #scroller { overflow: scroll; width: 500px; height: 400px}
        #block1 { height: 100px; width: 100px; overflow: hidden}
        #anchor { height: 1000px; }
    </style>
    <div id='changer1'></div>
    <div id='scroller'>
      <div id='changer2'></div>
      <div id='block1'></div>
      <div id='anchor'></div>
    </div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollableArea* scroller =
      ScrollerForElement(GetDocument().getElementById("scroller"));
  Element* changer1 = GetDocument().getElementById("changer1");
  Element* changer2 = GetDocument().getElementById("changer2");
  Element* anchor = GetDocument().getElementById("anchor");

  scroller->ScrollBy(ScrollOffset(0, 150), kUserScroll);
  ScrollLayoutViewport(ScrollOffset(0, 50));

  // In this layout pass both the inner and outer scroller will anchor to
  // #anchor.
  SetHeight(changer1, 100);
  SetHeight(changer2, 100);
  EXPECT_EQ(250, scroller->ScrollOffsetInt().Height());
  EXPECT_EQ(anchor->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  EXPECT_EQ(anchor->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // Test that the inner scroller can be destroyed without crashing.
  GetDocument().getElementById("scroller")->remove();
  Update();
}

TEST_P(ScrollAnchorTest, FlexboxDelayedClampingAlsoDelaysAdjustment) {
  SetBodyInnerHTML(R"HTML(
    <style>
        html { overflow: hidden; }
        body {
            position: absolute; display: flex;
            top: 0; bottom: 0; margin: 0;
        }
        #scroller { overflow: auto; }
        #spacer { width: 600px; height: 1200px; }
        #before { height: 50px; }
        #anchor {
            width: 100px; height: 100px;
            background-color: #8f8;
        }
    </style>
    <div id='scroller'>
        <div id='spacer'>
            <div id='before'></div>
            <div id='anchor'></div>
        </div>
    </div>
  )HTML");

  Element* scroller = GetDocument().getElementById("scroller");
  scroller->setScrollTop(100);

  SetHeight(GetDocument().getElementById("before"), 100);
  EXPECT_EQ(150, ScrollerForElement(scroller)->ScrollOffsetInt().Height());
}

TEST_P(ScrollAnchorTest, FlexboxDelayedAdjustmentRespectsSANACLAP) {
  SetBodyInnerHTML(R"HTML(
    <style>
        html { overflow: hidden; }
        body {
            position: absolute; display: flex;
            top: 0; bottom: 0; margin: 0;
        }
        #scroller { overflow: auto; }
        #spacer { width: 600px; height: 1200px; }
        #anchor {
            position: relative; top: 50px;
            width: 100px; height: 100px;
            background-color: #8f8;
        }
    </style>
    <div id='scroller'>
        <div id='spacer'>
            <div id='anchor'></div>
        </div>
    </div>
  )HTML");

  Element* scroller = GetDocument().getElementById("scroller");
  scroller->setScrollTop(100);

  GetDocument().getElementById("spacer")->setAttribute(HTMLNames::styleAttr,
                                                       "margin-top: 50px");
  Update();
  EXPECT_EQ(100, ScrollerForElement(scroller)->ScrollOffsetInt().Height());
}

// TODO(skobes): Convert this to web-platform-tests when document.rootScroller
// is launched (http://crbug.com/505516).
TEST_P(ScrollAnchorTest, NonDefaultRootScroller) {
  // TODO(crbug.com/889449): The test fails in LayoutNG mode.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
        ::-webkit-scrollbar {
          width: 0px; height: 0px;
        }
        body, html {
          margin: 0px; width: 100%; height: 100%;
        }
        #rootscroller {
          overflow: scroll; width: 100%; height: 100%;
        }
        .spacer {
          height: 600px; width: 100px;
        }
        #target {
          height: 100px; width: 100px; background-color: red;
        }
    </style>
    <div id='rootscroller'>
        <div id='firstChild' class='spacer'></div>
        <div id='target'></div>
        <div class='spacer'></div>
    </div>
    <div class='spacer'></div>
  )HTML");

  Element* root_scroller_element = GetDocument().getElementById("rootscroller");

  NonThrowableExceptionState non_throw;
  GetDocument().setRootScroller(root_scroller_element, non_throw);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ScrollableArea* scroller = ScrollerForElement(root_scroller_element);

  // By making the #rootScroller DIV the rootScroller, it should become the
  // layout viewport on the RootFrameViewport.
  ASSERT_EQ(scroller,
            &GetDocument().View()->GetRootFrameViewport()->LayoutViewport());

  // The #rootScroller DIV's anchor should have the RootFrameViewport set as
  // the scroller, rather than the FrameView's anchor.

  root_scroller_element->setScrollTop(600);

  SetHeight(GetDocument().getElementById("firstChild"), 1000);

  // Scroll anchoring should be applied to #rootScroller.
  EXPECT_EQ(1000, scroller->GetScrollOffset().Height());
  EXPECT_EQ(GetDocument().getElementById("target")->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  // Scroll anchoring should not apply within main frame.
  EXPECT_EQ(0, LayoutViewport()->GetScrollOffset().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(LayoutViewport()).AnchorObject());
}

// This test verifies that scroll anchoring is disabled when the document is in
// printing mode.
TEST_P(ScrollAnchorTest, AnchoringDisabledForPrinting) {
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } div { height: 100px } </style>
    <div id='block1'>abc</div>
    <div id='block2'>def</div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 150));

  // This will trigger printing and layout.
  PrintContext::NumberOfPages(GetDocument().GetFrame(), FloatSize(500, 500));

  EXPECT_EQ(150, viewport->ScrollOffsetInt().Height());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_P(ScrollAnchorTest, SerializeAnchorSimple) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div { height: 100px; }
      </style>
      <div id='block1'>abc</div>
      <div id='block2'>def</div>")HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  ValidateSerializedAnchor("#block2", LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorUsesTagname) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        span, a { display: block; height: 100px; }
      </style>
      <div id='ancestor'>
        <a class='foobar'>abc</a>
        <span class='barbaz'>def</span>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  ValidateSerializedAnchor("#ancestor>span", LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorSetsIsAnchorBit) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div { height: 100px; }
        .scroller {
          overflow: scroll;
          width: 400px;
          height: 400px;
        }
      </style>
      <div id='s1' class='scroller'>
        <div id='anchor'>abc</div>
      </div>")HTML");

  ScrollLayoutViewport(ScrollOffset(0, 50));
  ValidateSerializedAnchor("#anchor", LayoutPoint(0, -50));

  Element* s1 = GetDocument().getElementById("s1");
  Element* anchor = GetDocument().getElementById("anchor");
  // Remove the anchor. If the IsScrollAnchorOBject bit is set as it should be,
  // the anchor object will get cleaned up correctly.
  s1->RemoveChild(anchor);
  // Trigger a re-layout, which will crash if it wasn't properly cleaned up when
  // removing it from the DOM.
  ScrollLayoutViewport(ScrollOffset(0, 25));
}

TEST_P(ScrollAnchorTest, SerializeAnchorSetsSavedRelativeOffset) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div { height: 100px; }
      </style>
      <div id='block1'>abc</div>
      <div id='block2'>def</div>")HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  GetScrollAnchor(LayoutViewport()).Clear();
  ValidateSerializedAnchor("#block2", LayoutPoint(0, -50));

  SetHeight(GetDocument().getElementById("block1"), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 250);
}

TEST_P(ScrollAnchorTest, SerializeAnchorUsesClassname) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        span { display: block; height: 100px; }
      </style>
      <div id='ancestor'>
        <span class='foobar'>abc</span>
        <span class='barbaz'>def</span>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  ValidateSerializedAnchor("#ancestor>.barbaz", LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorUsesNthChild) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        p,span { display: block; height: 100px; }
      </style>
      <div id='ancestor'>
        <span class='foobar'>abc</span>
        <span class='foobar'>def</span>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  ValidateSerializedAnchor("#ancestor>:nth-child(2)", LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorUsesLeastSpecificSelector) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div.hundred { height: 100px; }
        div.thousand { height: 1000px; }
      </style>
      <div id='ancestor' class='thousand'>
       <div class='hundred'>abc</div>
       <div class='hundred'>def</div>
       <div class='hundred'>
         <div class='hundred foobar'>
           <div class='hundred'>ghi</div>
         </div>
       <div class='hundred barbaz'></div>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 250));
  ValidateSerializedAnchor("#ancestor>:nth-child(3)>.foobar>div",
                           LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorWithNoIdAttribute) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div.hundred { height: 100px; }
        div.thousand { height: 1000px; }
      </style>
      <div class='thousand'>
       <div class='hundred'>abc</div>
       <div class='hundred'>def</div>
       <div class='hundred'>
         <div class='hundred foobar'>
           <div class='hundred'>ghi</div>
         </div>
       <div class='hundred barbaz'></div>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 250));
  ValidateSerializedAnchor("html>body>div>:nth-child(3)>.foobar>div",
                           LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorChangesWithScroll) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        span { margin: 0; display: block; height: 100px; }
      </style>
      <div id='ancestor'>
        <span class='foobar'>abc</span>
        <span class='barbaz'>def</span>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 50));
  ValidateSerializedAnchor("#ancestor>.foobar", LayoutPoint(0, -50));

  ScrollLayoutViewport(ScrollOffset(0, 100));
  ValidateSerializedAnchor("#ancestor>.barbaz", LayoutPoint(0, -50));

  ScrollLayoutViewport(ScrollOffset(0, -100));
  ValidateSerializedAnchor("#ancestor>.foobar", LayoutPoint(0, -50));

  ScrollLayoutViewport(ScrollOffset(0, -49));
  ValidateSerializedAnchor("#ancestor>.foobar", LayoutPoint(0, -1));
}

TEST_P(ScrollAnchorTest, SerializeAnchorVerticalWritingMode) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body {
          height: 100px;
          width: 1000px;
          margin: 0;
          writing-mode:
          vertical-lr;
        }
        div { width: 100px; height: 100px; }
      </style>
      <div class = 'foobar'>abc</div>
      <div class = 'barbaz'>def</div>)HTML");

  ScrollLayoutViewport(ScrollOffset(50, 0));
  ValidateSerializedAnchor("html>body>.foobar", LayoutPoint(-50, 0));

  ScrollLayoutViewport(ScrollOffset(25, 0));
  ValidateSerializedAnchor("html>body>.foobar", LayoutPoint(-75, 0));

  ScrollLayoutViewport(ScrollOffset(75, 0));
  ValidateSerializedAnchor("html>body>.barbaz", LayoutPoint(-50, 0));
}

TEST_P(ScrollAnchorTest, SerializeAnchorQualifiedTagName) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        ns\\:div { height: 100px; display: block; }
      </style>
      <div style='height:100px'>foobar</div>
      <ns:div style='height: 100px; display: block;'
      xmlns:ns='http://www.w3.org/2005/Atom'>abc</ns:div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  ValidateSerializedAnchor("html>body>ns\\:div", LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorLimitsSelectorLength) {
  StringBuilder builder;
  builder.Append("<style> body { height: 1000px; margin: 0; }</style>");
  builder.Append("<div style='height:100px'>foobar</div>");
  builder.Append("<");
  for (int i = 0; i <= kMaxSerializedSelectorLength; i++) {
    builder.Append("a");
  }
  builder.Append(" style='display:block; height:100px;'/>");
  SetBodyInnerHTML(builder.ToString());

  ScrollLayoutViewport(ScrollOffset(0, 150));
  SerializedAnchor serialized =
      GetScrollAnchor(LayoutViewport()).GetSerializedAnchor();
  EXPECT_FALSE(serialized.IsValid());
}

TEST_P(ScrollAnchorTest, SerializeAnchorIgnoresDuplicatedId) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        span { display: block; height: 100px; }
      </style>
      <div id='ancestor'>
      </div>
      <div id='ancestor'>
        <span class='foobar'>abc</span>
        <span class='barbaz'>def</span>
      </div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 150));
  ValidateSerializedAnchor("html>body>:nth-child(3)>.barbaz",
                           LayoutPoint(0, -50));
}

TEST_P(ScrollAnchorTest, SerializeAnchorFailsForPseudoElement) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div { height: 100px }
        div:after { content: "foobar"; display: block; margin-top: 50px; }
      </style>
      <div>abc</div>
      <div id='block1'>def</div>)HTML");

  ScrollLayoutViewport(ScrollOffset(0, 50));
  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());
}

TEST_P(ScrollAnchorTest, RestoreAnchorSimple) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("#block2", LayoutPoint(0, 0));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 100);

  SetHeight(GetDocument().getElementById("block1"), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 200);

  SetHeight(GetDocument().getElementById("block1"), 50);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 50);
}

TEST_P(ScrollAnchorTest, RestoreAnchorNonTrivialSelector) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 1000px; margin: 0; }
        div.hundred { height: 100px; }
        div.thousand { height: 1000px; }
      </style>
      <div id='block1' class='hundred'>abc</div>
      <div id='ancestor' class='thousand'>
       <div class='hundred'>abc</div>
       <div class='hundred'>def</div>
       <div class='hundred'>
         <div class='hundred foobar'>
           <div class='hundred'>ghi</div>
         </div>
       <div class='hundred barbaz'></div>
      </div>)HTML");

  SerializedAnchor serialized_anchor("#ancestor>:nth-child(3)>.foobar>div",
                                     LayoutPoint(0, -50));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));

  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 350);

  SetHeight(GetDocument().getElementById("block1"), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 450);
}

TEST_P(ScrollAnchorTest, RestoreAnchorFailsForInvalidSelectors) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("article", LayoutPoint(0, 0));

  EXPECT_FALSE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));

  SerializedAnchor serialized_anchor_2("", LayoutPoint(0, 0));

  EXPECT_FALSE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor_2));

  SerializedAnchor serialized_anchor_3("foobar", LayoutPoint(0, 0));

  EXPECT_FALSE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor_3));
}

// Ensure that when the serialized selector refers to a non-box, non-text
// element(meaning its corresponding LayoutObject can't be the anchor object)
// that restoration will still succeed.
TEST_P(ScrollAnchorTest, RestoreAnchorSucceedsForNonBoxNonTextElement) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<code>some code</code>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("html>body>code", LayoutPoint(0, 0));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));

  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 100);

  SetHeight(GetDocument().getElementById("block1"), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 200);

  SerializedAnchor serialized =
      GetScrollAnchor(LayoutViewport()).GetSerializedAnchor();
  ValidateSerializedAnchor("html>body>code", LayoutPoint(0, 0));
}

TEST_P(ScrollAnchorTest, RestoreAnchorSucceedsWhenScriptForbidden) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("#block2", LayoutPoint(0, 0));

  ScriptForbiddenScope scope;
  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 100);
}

TEST_P(ScrollAnchorTest, RestoreAnchorSucceedsWithExistingAnchorObject) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("#block1", LayoutPoint(0, 0));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_TRUE(GetScrollAnchor(LayoutViewport()).AnchorObject());
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 0);

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_TRUE(GetScrollAnchor(LayoutViewport()).AnchorObject());
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().Height(), 0);
}

TEST_P(ScrollAnchorTest, DeleteAnonymousBlockCrash) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <div id="deleteMe" style="height:20000px;"></div>
      torsk
    </div>
  )HTML");

  // Removing #deleteMe will also remove the anonymous block around the text
  // node. This would cause NG to point to dead layout objects, prior to
  // https://chromium-review.googlesource.com/1193868 and therefore crash.

  ScrollLayoutViewport(ScrollOffset(0, 20000));
  GetDocument().getElementById("deleteMe")->remove();
  Update();
}
}
