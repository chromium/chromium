// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/scroll_anchor.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using Corner = ScrollAnchor::Corner;

class ScrollAnchorTest : public SimTest {
 public:
  ScrollAnchorTest() = default;

 protected:
  void SetUp() override {
    SimTest::SetUp();
    ResizeView(gfx::Size(800, 600));
    String kUrl = "https://example.com/test.html";
    SimRequest request(kUrl, "text/html");
    LoadURL(kUrl);
    request.Complete("<!DOCTYPE html>");
  }

  void Update() { Compositor().BeginFrame(); }

  void SetBodyInnerHTML(const String& body_content) {
    GetDocument().body()->setInnerHTML(body_content, ASSERT_NO_EXCEPTION);
    Update();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  VisualViewport& GetVisualViewport() {
    return GetDocument().View()->GetPage()->GetVisualViewport();
  }

  ScrollableArea* ScrollerForElement(Element* element) {
    return To<LayoutBox>(element->GetLayoutObject())->GetScrollableArea();
  }

  ScrollAnchor& GetScrollAnchor(ScrollableArea* scroller) {
    DCHECK(scroller->IsPaintLayerScrollableArea());
    return *(scroller->GetScrollAnchor());
  }

  void SetHeight(Element* element, int height) {
    element->setAttribute(html_names::kStyleAttr,
                          AtomicString(String::Format("height: %dpx", height)));
    Update();
  }

  void ScrollLayoutViewport(ScrollOffset delta) {
    Element* scrolling_element = GetDocument().scrollingElement();
    if (delta.x()) {
      scrolling_element->setScrollLeft(scrolling_element->scrollLeft() +
                                       delta.x());
    }
    if (delta.y()) {
      scrolling_element->setScrollTop(scrolling_element->scrollTop() +
                                      delta.y());
    }
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

  Scrollbar* VerticalScrollbarForElement(Element* element) {
    return ScrollerForElement(element)->VerticalScrollbar();
  }

  void MouseDownOnVerticalScrollbar(Scrollbar* scrollbar) {
    DCHECK_EQ(true, scrollbar->GetTheme().AllowsHitTest());
    int thumb_center = scrollbar->GetTheme().ThumbPosition(*scrollbar) +
                       scrollbar->GetTheme().ThumbLength(*scrollbar) / 2;
    scrollbar_drag_point_ =
        gfx::PointF(scrollbar->GetLayoutBox()
                        ->GetScrollableArea()
                        ->ConvertFromScrollbarToContainingEmbeddedContentView(
                            *scrollbar, gfx::Point(0, thumb_center)));
    scrollbar->MouseDown(blink::WebMouseEvent(
        blink::WebInputEvent::Type::kMouseDown, *scrollbar_drag_point_,
        *scrollbar_drag_point_, blink::WebPointerProperties::Button::kLeft, 0,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
  }

  void MouseDragVerticalScrollbar(Scrollbar* scrollbar, float scroll_delta_y) {
    DCHECK(scrollbar_drag_point_);
    ScrollableArea* scroller = scrollbar->GetLayoutBox()->GetScrollableArea();
    scrollbar_drag_point_->Offset(
        0, scroll_delta_y *
               (scrollbar->GetTheme().TrackLength(*scrollbar) -
                scrollbar->GetTheme().ThumbLength(*scrollbar)) /
               (scroller->MaximumScrollOffset().y() -
                scroller->MinimumScrollOffset().y()));
    scrollbar->MouseMoved(blink::WebMouseEvent(
        blink::WebInputEvent::Type::kMouseMove, *scrollbar_drag_point_,
        *scrollbar_drag_point_, blink::WebPointerProperties::Button::kLeft, 0,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
  }

  void MouseUpOnVerticalScrollbar(Scrollbar* scrollbar) {
    DCHECK(scrollbar_drag_point_);
    scrollbar->MouseDown(blink::WebMouseEvent(
        blink::WebInputEvent::Type::kMouseUp, *scrollbar_drag_point_,
        *scrollbar_drag_point_, blink::WebPointerProperties::Button::kLeft, 0,
        blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now()));
    scrollbar_drag_point_.reset();
  }

  std::optional<gfx::PointF> scrollbar_drag_point_;
};

// TODO(skobes): Convert this to web-platform-tests when visual viewport API is
// launched (http://crbug.com/635031).
TEST_F(ScrollAnchorTest, VisualViewportAnchors) {
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
  int top = GetDocument()
                .getElementById(AtomicString("text"))
                ->GetBoundingClientRect()
                ->top();
  v_viewport.SetLocation(gfx::PointF(0, top));

  SetHeight(GetDocument().getElementById(AtomicString("div")), 10);
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("text"))->GetLayoutObject(),
      GetScrollAnchor(l_viewport).AnchorObject());
  EXPECT_EQ(top - 90, v_viewport.ScrollOffsetInt().y());

  SetHeight(GetDocument().getElementById(AtomicString("div")), 100);
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("text"))->GetLayoutObject(),
      GetScrollAnchor(l_viewport).AnchorObject());
  EXPECT_EQ(top, v_viewport.ScrollOffsetInt().y());

  // Scrolling the visual viewport should clear the anchor.
  v_viewport.SetLocation(gfx::PointF(0, 0));
  EXPECT_EQ(nullptr, GetScrollAnchor(l_viewport).AnchorObject());
}

// Test that a non-anchoring scroll on scroller clears scroll anchors for all
// parent scrollers.
TEST_F(ScrollAnchorTest, ClearScrollAnchorsOnAncestors) {
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
  SetHeight(GetDocument().getElementById(AtomicString("changer")), 300);

  EXPECT_EQ(350, viewport->ScrollOffsetInt().y());
  EXPECT_EQ(
      GetDocument().getElementById(AtomicString("anchor"))->GetLayoutObject(),
      GetScrollAnchor(viewport).AnchorObject());

  // Scrolling the nested scroller should clear the anchor on the main frame.
  ScrollableArea* scroller = ScrollerForElement(
      GetDocument().getElementById(AtomicString("scroller")));
  scroller->ScrollBy(ScrollOffset(0, 100), mojom::blink::ScrollType::kUser);
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_F(ScrollAnchorTest, AncestorClearingWithSiblingReference) {
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
  Element* s1 = GetDocument().getElementById(AtomicString("s1"));
  Element* s2 = GetDocument().getElementById(AtomicString("s2"));
  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));

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

TEST_F(ScrollAnchorTest, FractionalOffsetsAreRoundedBeforeComparing) {
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } </style>
    <div id='block1' style='height: 50.4px'>abc</div>
    <div id='block2' style='height: 100px'>def</div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 100));

  GetDocument()
      .getElementById(AtomicString("block1"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 50.6px"));
  Update();

  EXPECT_EQ(101, viewport->ScrollOffsetInt().y());
}

TEST_F(ScrollAnchorTest, AvoidStickyAnchorWhichMovesWithScroll) {
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } </style>
    <div id='block1' style='height: 50px'>abc</div>
    <div id='block2' style='height: 100px; position: sticky; top: 0;'>
        def</div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 60));

  GetDocument()
      .getElementById(AtomicString("block1"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 100px"));
  Update();

  EXPECT_EQ(60, viewport->ScrollOffsetInt().y());
}

TEST_F(ScrollAnchorTest, AnchorWithLayerInScrollingDiv) {
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

  ScrollableArea* scroller = ScrollerForElement(
      GetDocument().getElementById(AtomicString("scroller")));
  Element* block1 = GetDocument().getElementById(AtomicString("block1"));
  Element* block2 = GetDocument().getElementById(AtomicString("block2"));

  scroller->ScrollBy(ScrollOffset(0, 150), mojom::blink::ScrollType::kUser);

  // In this layout pass we will anchor to #block2 which has its own PaintLayer.
  SetHeight(block1, 200);
  EXPECT_EQ(250, scroller->ScrollOffsetInt().y());
  EXPECT_EQ(block2->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());

  // Test that the anchor object can be destroyed without affecting the scroll
  // position.
  block2->remove();
  Update();
  EXPECT_EQ(250, scroller->ScrollOffsetInt().y());
}

TEST_F(ScrollAnchorTest, AnchorWhileDraggingScrollbar) {
  // Dragging the scrollbar is inherently inaccurate. Allow many pixels slop in
  // the scroll position.
  const int kScrollbarDragAccuracy = 10;
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
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
  Element* scroller_element =
      GetDocument().getElementById(AtomicString("scroller"));
  ScrollableArea* scroller = ScrollerForElement(scroller_element);

  Element* block1 = GetDocument().getElementById(AtomicString("block1"));
  Element* block2 = GetDocument().getElementById(AtomicString("block2"));

  Scrollbar* scrollbar = VerticalScrollbarForElement(scroller_element);
  scroller->MouseEnteredScrollbar(*scrollbar);
  MouseDownOnVerticalScrollbar(scrollbar);
  MouseDragVerticalScrollbar(scrollbar, 150);

  // Process the injected scroll gestures.
  GetWebFrameWidget().FlushInputHandlerTasks();
  Compositor().BeginFrame();

  EXPECT_NEAR(150, scroller->GetScrollOffset().y(), kScrollbarDragAccuracy);

  // In this layout pass we will anchor to #block2 which has its own PaintLayer.
  SetHeight(block1, 200);
  EXPECT_NEAR(250, scroller->ScrollOffsetInt().y(), kScrollbarDragAccuracy);
  EXPECT_EQ(block2->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());

  // If we continue dragging the scroller should scroll from the newly anchored
  // position.
  MouseDragVerticalScrollbar(scrollbar, 12);

  // Process the injected scroll gesture.
  GetWebFrameWidget().FlushInputHandlerTasks();
  Compositor().BeginFrame();

  EXPECT_NEAR(262, scroller->ScrollOffsetInt().y(), kScrollbarDragAccuracy);
  MouseUpOnVerticalScrollbar(scrollbar);
}

// Verify that a nested scroller with a div that has its own PaintLayer can be
// removed without causing a crash. This test passes if it doesn't crash.
TEST_F(ScrollAnchorTest, RemoveScrollerWithLayerInScrollingDiv) {
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
  ScrollableArea* scroller = ScrollerForElement(
      GetDocument().getElementById(AtomicString("scroller")));
  Element* changer1 = GetDocument().getElementById(AtomicString("changer1"));
  Element* changer2 = GetDocument().getElementById(AtomicString("changer2"));
  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));

  scroller->ScrollBy(ScrollOffset(0, 150), mojom::blink::ScrollType::kUser);
  ScrollLayoutViewport(ScrollOffset(0, 50));

  // In this layout pass both the inner and outer scroller will anchor to
  // #anchor.
  SetHeight(changer1, 100);
  SetHeight(changer2, 100);
  EXPECT_EQ(250, scroller->ScrollOffsetInt().y());
  EXPECT_EQ(anchor->GetLayoutObject(),
            GetScrollAnchor(scroller).AnchorObject());
  EXPECT_EQ(anchor->GetLayoutObject(),
            GetScrollAnchor(viewport).AnchorObject());

  // Test that the inner scroller can be destroyed without crashing.
  GetDocument().getElementById(AtomicString("scroller"))->remove();
  Update();
}

TEST_F(ScrollAnchorTest, FlexboxDelayedClampingAlsoDelaysAdjustment) {
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

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->setScrollTop(100);

  SetHeight(GetDocument().getElementById(AtomicString("before")), 100);
  EXPECT_EQ(150, ScrollerForElement(scroller)->ScrollOffsetInt().y());
}

TEST_F(ScrollAnchorTest, FlexboxDelayedAdjustmentRespectsSANACLAP) {
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

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->setScrollTop(100);

  GetDocument()
      .getElementById(AtomicString("spacer"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("margin-top: 50px"));
  Update();
  EXPECT_EQ(100, ScrollerForElement(scroller)->ScrollOffsetInt().y());
}

// This test verifies that scroll anchoring is disabled when the document is in
// printing mode.
TEST_F(ScrollAnchorTest, AnchoringDisabledForPrinting) {
  SetBodyInnerHTML(R"HTML(
    <style> body { height: 1000px } div { height: 100px } </style>
    <div id='block1'>abc</div>
    <div id='block2'>def</div>
  )HTML");

  ScrollableArea* viewport = LayoutViewport();
  ScrollLayoutViewport(ScrollOffset(0, 150));

  // This will trigger printing and layout.
  PrintContext::NumberOfPages(GetDocument().GetFrame(), gfx::SizeF(500, 500));

  EXPECT_EQ(150, viewport->ScrollOffsetInt().y());
  EXPECT_EQ(nullptr, GetScrollAnchor(viewport).AnchorObject());
}

TEST_F(ScrollAnchorTest, SerializeAnchorSimple) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorUsesTagname) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorSetsIsAnchorBit) {
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

  Element* s1 = GetDocument().getElementById(AtomicString("s1"));
  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));
  // Remove the anchor. If the IsScrollAnchorObject bit is set as it should be,
  // the anchor object will get cleaned up correctly.
  s1->RemoveChild(anchor);
  // Trigger a re-layout, which will crash if it wasn't properly cleaned up when
  // removing it from the DOM.
  ScrollLayoutViewport(ScrollOffset(0, 25));
}

TEST_F(ScrollAnchorTest, SerializeAnchorSetsSavedRelativeOffset) {
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

  SetHeight(GetDocument().getElementById(AtomicString("block1")), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 250);
}

TEST_F(ScrollAnchorTest, SerializeAnchorUsesClassname) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorUsesNthChild) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorUsesLeastSpecificSelector) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorWithNoIdAttribute) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorChangesWithScroll) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorVerticalWritingMode) {
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

TEST_F(ScrollAnchorTest, RestoreAnchorVerticalRlWritingMode) {
  SetBodyInnerHTML(R"HTML(
      <style>
      body {
          height: 100px;
          margin: 0;
          writing-mode:
          vertical-rl;
        }
        div.big { width: 800px; }
        div { width: 100px; height: 100px; }
      </style>
      <div class='big'></div>
      <div id='last'></div>
      )HTML");

  SerializedAnchor serialized_anchor("#last", LayoutPoint(0, 0));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().x(), 0);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 0);
}

TEST_F(ScrollAnchorTest, SerializeAnchorQualifiedTagName) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorLimitsSelectorLength) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorIgnoresDuplicatedId) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorFailsForPseudoElement) {
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

TEST_F(ScrollAnchorTest, SerializeAnchorFailsForShadowDOMElement) {
  SetBodyInnerHTML(R"HTML(
      <style>
        body { height: 5000px; margin: 0; }
        div { height: 200px; }
      </style>
      <div id='host'></div>
      <div></div>
      <div></div>)HTML");
  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto& shadow_root = host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
      <style>
        div { height: 100px; }
      </style>
      <div></div>)HTML");
  Update();

  ScrollLayoutViewport(ScrollOffset(0, 50));

  SerializedAnchor serialized =
      GetScrollAnchor(LayoutViewport()).GetSerializedAnchor();
  EXPECT_FALSE(serialized.IsValid());

  LayoutObject* anchor_object =
      GetScrollAnchor(LayoutViewport()).AnchorObject();
  EXPECT_TRUE(anchor_object->GetNode()->IsInShadowTree());
}

TEST_F(ScrollAnchorTest, RestoreAnchorSimple) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<div id='block2'>def</div>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("#block2", LayoutPoint(0, 0));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 100);

  SetHeight(GetDocument().getElementById(AtomicString("block1")), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 200);

  SetHeight(GetDocument().getElementById(AtomicString("block1")), 50);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 50);
}

TEST_F(ScrollAnchorTest, RestoreAnchorNonTrivialSelector) {
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

  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 350);

  SetHeight(GetDocument().getElementById(AtomicString("block1")), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 450);
}

TEST_F(ScrollAnchorTest, RestoreAnchorFailsForInvalidSelectors) {
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
TEST_F(ScrollAnchorTest, RestoreAnchorSucceedsForNonBoxNonTextElement) {
  SetBodyInnerHTML(
      "<style> body { height: 1000px; margin: 0; } div { height: 100px } "
      "</style>"
      "<div id='block1'>abc</div>"
      "<code>some code</code>");

  EXPECT_FALSE(GetScrollAnchor(LayoutViewport()).AnchorObject());

  SerializedAnchor serialized_anchor("html>body>code", LayoutPoint(0, 0));

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));

  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 100);

  SetHeight(GetDocument().getElementById(AtomicString("block1")), 200);
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 200);

  SerializedAnchor serialized =
      GetScrollAnchor(LayoutViewport()).GetSerializedAnchor();
  ValidateSerializedAnchor("html>body>code", LayoutPoint(0, 0));
}

TEST_F(ScrollAnchorTest, RestoreAnchorSucceedsWhenScriptForbidden) {
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
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 100);
}

TEST_F(ScrollAnchorTest, RestoreAnchorSucceedsWithExistingAnchorObject) {
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
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 0);

  EXPECT_TRUE(
      GetScrollAnchor(LayoutViewport()).RestoreAnchor(serialized_anchor));
  EXPECT_TRUE(GetScrollAnchor(LayoutViewport()).AnchorObject());
  EXPECT_EQ(LayoutViewport()->ScrollOffsetInt().y(), 0);
}

TEST_F(ScrollAnchorTest, DeleteAnonymousBlockCrash) {
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
  GetDocument().getElementById(AtomicString("deleteMe"))->remove();
  Update();
}

TEST_F(ScrollAnchorTest, ClampAdjustsAnchorAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      .content {
        height: 45vh;
        background: lightblue;
      }
      #hidden {
        height: 200px;
        display: none;
      }
    </style>
    <div class="content" id=one></div>
    <div id="hidden"></div>
    <div class="content" id=two></div>
    <div class="content" id=three></div>
    <div class="content" id=four></div>
  )HTML");
  LayoutViewport()->SetScrollOffset(ScrollOffset(0, 2000),
                                    mojom::blink::ScrollType::kUser);
  Update();
  GetDocument()
      .getElementById(AtomicString("hidden"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("display:block"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ(gfx::Vector2d(0, 200),
            LayoutViewport()
                ->GetScrollAnimator()
                .ImplOnlyAnimationAdjustmentForTesting());
  GetDocument()
      .getElementById(AtomicString("hidden"))
      ->setAttribute(html_names::kStyleAttr, g_empty_atom);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // The clamping scroll after resizing scrollable overflow to be smaller
  // should adjust the animation back to 0.
  EXPECT_EQ(gfx::Vector2d(0, 0), LayoutViewport()
                                     ->GetScrollAnimator()
                                     .ImplOnlyAnimationAdjustmentForTesting());
}

// crbug.com/1413945
TEST_F(ScrollAnchorTest, DynamicMultiColumnCrash) {
  SetBodyInnerHTML(R"HTML(
    <div id="id125" style="container:foo/size; overflow-y:hidden;
        writing-mode:vertical-rl;">
    x</div>)HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Element* target = GetDocument().getElementById(AtomicString("id125"));
  target->SetInlineStyleProperty(CSSPropertyID::kFontSize, "0");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  target->SetInlineStyleProperty(CSSPropertyID::kColumns, "2");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // Pass if no crashes.
}

class ScrollAnchorTestFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  ~ScrollAnchorTestFindInPageClient() override = default;

  void SetFrame(WebLocalFrameImpl* frame) {
    frame->GetFindInPage()->SetClient(receiver_.BindNewPipeAndPassRemote());
  }

  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      mojom::blink::FindMatchUpdateType final_update) final {
    count_ = current_number_of_matches;
  }

  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      mojom::blink::FindMatchUpdateType final_update) final {}

  int Count() const { return count_; }
  void Reset() { count_ = -1; }

 private:
  int count_ = -1;
  mojo::Receiver<mojom::blink::FindInPageClient> receiver_{this};
};

class ScrollAnchorFindInPageTest : public testing::Test {
 public:
  void SetUp() override { web_view_helper_.Initialize(); }
  void TearDown() override { web_view_helper_.Reset(); }

  Document& GetDocument() {
    return *static_cast<Document*>(
        web_view_helper_.LocalMainFrame()->GetDocument());
  }
  FindInPage* GetFindInPage() {
    return web_view_helper_.LocalMainFrame()->GetFindInPage();
  }
  WebLocalFrameImpl* LocalMainFrame() {
    return web_view_helper_.LocalMainFrame();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  void SetHtmlInnerHTML(const char* content) {
    GetDocument().documentElement()->setInnerHTML(String::FromUTF8(content));
    UpdateAllLifecyclePhasesForTest();
  }

  void ResizeAndFocus() {
    web_view_helper_.Resize(gfx::Size(640, 480));
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    test::RunPendingTasks();
  }

  mojom::blink::FindOptionsPtr FindOptions(bool new_session = true) {
    auto find_options = mojom::blink::FindOptions::New();
    find_options->run_synchronously_for_testing = true;
    find_options->new_session = new_session;
    find_options->forward = true;
    return find_options;
  }

  void Find(String search_text,
            ScrollAnchorTestFindInPageClient& client,
            bool new_session = true) {
    client.Reset();
    GetFindInPage()->Find(FAKE_FIND_ID, search_text, FindOptions(new_session));
    test::RunPendingTasks();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  const int FAKE_FIND_ID = 1;

 private:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(ScrollAnchorFindInPageTest, FindInPageResultPrioritized) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    body { height: 4000px }
    .spacer { height: 100px }
    #growing { height: 100px }
    </style>

    <div class=spacer></div>
    <div class=spacer></div>
    <div class=spacer></div>
    <div class=spacer></div>
    <div id=growing></div>
    <div class=spacer></div>
    <div id=target>findme</div>
    <div class=spacer></div>
    <div class=spacer></div>
  )HTML");

  LayoutViewport()->SetScrollOffset(ScrollOffset(0, 150),
                                    mojom::blink::ScrollType::kUser);

  const String search_text = "findme";
  ScrollAnchorTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find(search_text, client);
  ASSERT_EQ(1, client.Count());

  // Save the old bounds for comparison.
  auto* old_bounds = GetDocument()
                         .getElementById(AtomicString("target"))
                         ->GetBoundingClientRect();

  GetDocument()
      .getElementById(AtomicString("growing"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 3000px"));
  UpdateAllLifecyclePhasesForTest();

  auto* new_bounds = GetDocument()
                         .getElementById(AtomicString("target"))
                         ->GetBoundingClientRect();

  // The y coordinate of the target should not change.
  EXPECT_EQ(old_bounds->y(), new_bounds->y());
}

TEST_F(ScrollAnchorFindInPageTest, FocusPrioritizedOverFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    body { height: 4000px }
    .spacer { height: 100px }
    #growing { height: 100px }
    #focus_target { height: 10px }
    </style>

    <div class=spacer></div>
    <div class=spacer></div>
    <div class=spacer></div>
    <div class=spacer></div>
    <div id=focus_target contenteditable></div>
    <div id=growing></div>
    <div id=find_target>findme</div>
    <div class=spacer></div>
    <div class=spacer></div>
  )HTML");

  LayoutViewport()->SetScrollOffset(ScrollOffset(0, 150),
                                    mojom::blink::ScrollType::kUser);

  const String search_text = "findme";
  ScrollAnchorTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  Find(search_text, client);
  ASSERT_EQ(1, client.Count());

  GetDocument().getElementById(AtomicString("focus_target"))->Focus();

  // Save the old bounds for comparison.
  auto* old_focus_bounds = GetDocument()
                               .getElementById(AtomicString("focus_target"))
                               ->GetBoundingClientRect();
  auto* old_find_bounds = GetDocument()
                              .getElementById(AtomicString("find_target"))
                              ->GetBoundingClientRect();

  GetDocument()
      .getElementById(AtomicString("growing"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 3000px"));
  UpdateAllLifecyclePhasesForTest();

  auto* new_focus_bounds = GetDocument()
                               .getElementById(AtomicString("focus_target"))
                               ->GetBoundingClientRect();
  auto* new_find_bounds = GetDocument()
                              .getElementById(AtomicString("find_target"))
                              ->GetBoundingClientRect();

  // `focus_target` should remain where it is, since it is prioritized.
  // `find_target`, however, is shifted.
  EXPECT_EQ(old_focus_bounds->y(), new_focus_bounds->y());
  EXPECT_NE(old_find_bounds->y(), new_find_bounds->y());
}

TEST_F(ScrollAnchorFindInPageTest, FocusedUnderStickyIsSkipped) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    body { height: 4000px; position: relative; }
    .spacer { height: 100px }
    #growing { height: 100px }
    .sticky { position: sticky; top: 10px; }
    #target { width: 10px; height: 10px; }
    </style>

    <div class=spacer></div>
    <div class=spacer></div>
    <div class=spacer></div>
    <div class=spacer></div>
    <div id=growing></div>
    <div class=spacer></div>
    <div id=check></div>
    <div class=sticky>
      <div id=target contenteditable></div>
    </div>
    <div class=spacer></div>
    <div class=spacer></div>
  )HTML");

  LayoutViewport()->SetScrollOffset(ScrollOffset(0, 150),
                                    mojom::blink::ScrollType::kUser);

  GetDocument().getElementById(AtomicString("target"))->Focus();

  // Save the old bounds for comparison. Use #check, since sticky won't move
  // regardless of scroll anchoring.
  auto* old_bounds = GetDocument()
                         .getElementById(AtomicString("check"))
                         ->GetBoundingClientRect();

  GetDocument()
      .getElementById(AtomicString("growing"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 3000px"));
  UpdateAllLifecyclePhasesForTest();

  auto* new_bounds = GetDocument()
                         .getElementById(AtomicString("check"))
                         ->GetBoundingClientRect();

  // The y coordinate of #check should change since #target is not a valid
  // anchor, so we should have selected one of the spacers as the anchor.
  EXPECT_NE(old_bounds->y(), new_bounds->y());
}

class ScrollAnchorPageTest : public RenderingTest {};

// crbug.com/1443633
TEST_F(ScrollAnchorPageTest, SvgRelativeBoundsCrashAfterClearLayoutResults) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
<style>body { font-size: 18px; }</style>
<div style="overflow:auto; columns:1; column-fill:auto; width:300px; height:350px;">
  <svg viewbox="0 0 100 100">
    <foreignObject style="width:100px; height:2px;">
      <span id="target"><br>foo</span>
    </foreignObject>
  </svg>
  <div id="scrollbarSummoner" style="display:none;">
    <div style="height:200px;"></div>
  </div>
</div>)HTML");
  Document& doc = GetDocument();
  doc.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  doc.getElementById(AtomicString("target"))->scrollIntoView();
  doc.getElementById(AtomicString("scrollbarSummoner"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("display:block; contain:size; height:0"));

  // During the following layout, ClearLayoutResults() for the first <div> was
  // called, then ScrollAnchor::NotifyBeforeLayout() for <foreignObject> was
  // called. It accessed the geometry of the first <div>.
  doc.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // Pass if no crashes.
}
}
