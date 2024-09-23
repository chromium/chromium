// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

class SpatialNavigationTest : public RenderingTest {
 public:
  SpatialNavigationTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    RenderingTest::SetUp();
    GetDocument().GetSettings()->SetSpatialNavigationEnabled(true);
  }

  PhysicalRect TopOfVisualViewport() {
    PhysicalRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetY(visual_viewport.Y() - 1);
    visual_viewport.SetHeight(LayoutUnit(0));
    return visual_viewport;
  }

  PhysicalRect BottomOfVisualViewport() {
    PhysicalRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetY(visual_viewport.Bottom() + 1);
    visual_viewport.SetHeight(LayoutUnit(0));
    return visual_viewport;
  }

  PhysicalRect LeftSideOfVisualViewport() {
    PhysicalRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetX(visual_viewport.X() - 1);
    visual_viewport.SetWidth(LayoutUnit(0));
    return visual_viewport;
  }

  PhysicalRect RightSideOfVisualViewport() {
    PhysicalRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetX(visual_viewport.Right() + 1);
    visual_viewport.SetWidth(LayoutUnit(0));
    return visual_viewport;
  }

  void AssertUseSidesOfVisualViewport(Node* focus_node) {
    EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), focus_node,
                           SpatialNavigationDirection::kUp),
              BottomOfVisualViewport());
    EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), focus_node,
                           SpatialNavigationDirection::kDown),
              TopOfVisualViewport());
    EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), focus_node,
                           SpatialNavigationDirection::kLeft),
              RightSideOfVisualViewport());
    EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), focus_node,
                           SpatialNavigationDirection::kRight),
              LeftSideOfVisualViewport());
  }

  void AssertNormalizedHeight(Element* e, int line_height, bool will_shrink) {
    PhysicalRect search_origin =
        SearchOrigin(RootViewport(e->GetDocument().GetFrame()), e,
                     SpatialNavigationDirection::kDown);
    PhysicalRect uncropped = NodeRectInRootFrame(e);

    // SearchOrigin uses the normalized height.
    // If |e| is line broken, SearchOrigin should only use the first line.
    PhysicalRect normalized =
        ShrinkInlineBoxToLineBox(*e->GetLayoutObject(), uncropped);
    EXPECT_EQ(search_origin, normalized);
    if (will_shrink) {
      EXPECT_LT(search_origin.Height(), uncropped.Height());
      EXPECT_EQ(search_origin.Height(), line_height);
      EXPECT_EQ(search_origin.X(), uncropped.X());
      EXPECT_EQ(search_origin.Y(), uncropped.Y());
      EXPECT_EQ(search_origin.Width(), uncropped.Width());
    } else {
      EXPECT_EQ(search_origin, uncropped);
    }

    // Focus candidates will also use normalized heights.
    // If |e| is line broken, the rect should still include all lines.
    normalized = ShrinkInlineBoxToLineBox(*e->GetLayoutObject(), uncropped,
                                          LineBoxes(*e->GetLayoutObject()));
    FocusCandidate candidate(e, SpatialNavigationDirection::kDown);
    EXPECT_EQ(normalized, candidate.rect_in_root_frame);
  }

  bool HasSameSearchOriginRectAndCandidateRect(Element* a) {
    PhysicalRect a_origin =
        SearchOrigin(RootViewport(a->GetDocument().GetFrame()), a,
                     SpatialNavigationDirection::kDown);
    FocusCandidate a_candidate(a, SpatialNavigationDirection::kDown);
    return a_candidate.rect_in_root_frame == a_origin;
  }

  bool Intersects(Element* a, Element* b) {
    PhysicalRect a_origin =
        SearchOrigin(RootViewport(a->GetDocument().GetFrame()), a,
                     SpatialNavigationDirection::kDown);
    PhysicalRect b_origin =
        SearchOrigin(RootViewport(b->GetDocument().GetFrame()), b,
                     SpatialNavigationDirection::kDown);

    return a_origin.Intersects(b_origin);
  }
};

TEST_F(SpatialNavigationTest, RootFramesVisualViewport) {
  // Test RootViewport with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(gfx::PointF(200, 200));

  LocalFrameView* root_frame_view = GetFrame().LocalFrameRoot().View();
  const PhysicalRect roots_visible_doc_rect(
      root_frame_view->GetScrollableArea()->VisibleContentRect());
  // Convert the root frame's visible rect from document space -> frame space.
  // For the root frame, frame space == root frame space, obviously.
  PhysicalRect viewport_rect_of_root_frame =
      root_frame_view->DocumentToFrame(roots_visible_doc_rect);

  EXPECT_EQ(viewport_rect_of_root_frame, RootViewport(&GetFrame()));
}

TEST_F(SpatialNavigationTest, FindContainerWhenEnclosingContainerIsDocument) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<a id='child'>link</a>");

  Element* child_element = GetDocument().getElementById(AtomicString("child"));
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);

  EXPECT_EQ(enclosing_container, GetDocument());
  EXPECT_TRUE(IsScrollableAreaOrDocument(enclosing_container));
}

TEST_F(SpatialNavigationTest, FindContainerWhenEnclosingContainerIsIframe) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  iframe {"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<a>link</a>");

  UpdateAllLifecyclePhasesForTest();
  Element* iframe = GetDocument().QuerySelector(AtomicString("iframe"));
  Element* link = ChildDocument().QuerySelector(AtomicString("a"));
  Node* enclosing_container = ScrollableAreaOrDocumentOf(link);

  EXPECT_FALSE(IsOffscreen(iframe));
  EXPECT_FALSE(IsOffscreen(&ChildDocument()));
  EXPECT_FALSE(IsOffscreen(link));

  EXPECT_EQ(enclosing_container, ChildDocument());
  EXPECT_TRUE(IsScrollableAreaOrDocument(enclosing_container));
}

TEST_F(SpatialNavigationTest,
       FindContainerWhenEnclosingContainerIsScrollableOverflowBox) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #content {"
      "    margin-top: 200px;"  // Outside the div's viewport.
      "  }"
      "  #container {"
      "    height: 100px;"
      "    overflow: scroll;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='content'>some text here</div>"
      "</div>");

  Element* content = GetDocument().getElementById(AtomicString("content"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Node* enclosing_container = ScrollableAreaOrDocumentOf(content);

  // TODO(crbug.com/889840):
  // VisibleBoundsInLocalRoot does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInLocalRoot, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(content->VisibleBoundsInLocalRoot().IsEmpty());  // EXPECT_TRUE.

  EXPECT_TRUE(IsOffscreen(content));
  EXPECT_FALSE(IsOffscreen(container));

  EXPECT_EQ(enclosing_container, container);
  EXPECT_TRUE(IsScrollableAreaOrDocument(enclosing_container));
}

TEST_F(SpatialNavigationTest, ZooomPutsElementOffScreen) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<button id='a'>hello</button><br>"
      "<button id='b' style='margin-top: 70%'>bello</button>");

  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  EXPECT_FALSE(IsOffscreen(a));
  EXPECT_FALSE(IsOffscreen(b));

  // Now, test IsOffscreen with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  // #b is no longer visible.
  EXPECT_FALSE(IsOffscreen(a));
  EXPECT_TRUE(IsOffscreen(b));
}

TEST_F(SpatialNavigationTest, RootViewportRespectsVisibleSize) {
  EXPECT_EQ(RootViewport(&GetFrame()), PhysicalRect(0, 0, 800, 600));

  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetSize({123, 123});
  EXPECT_EQ(RootViewport(&GetFrame()), PhysicalRect(0, 0, 123, 123));
}

TEST_F(SpatialNavigationTest, StartAtVisibleFocusedElement) {
  SetBodyInnerHTML("<button id='b'>hello</button>");
  Element* b = GetDocument().getElementById(AtomicString("b"));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kDown),
            NodeRectInRootFrame(b));
}

TEST_F(SpatialNavigationTest, StartAtVisibleFocusedScroller) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #content {"
      "    margin-top: 200px;"  // Outside the div's viewport.
      "  }"
      "  #scroller {"
      "    height: 100px;"
      "    overflow: scroll;"
      "  }"
      "</style>"
      "<div id='scroller'>"
      "  <div id='content'>some text here</div>"
      "</div>");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), scroller,
                         SpatialNavigationDirection::kDown),
            NodeRectInRootFrame(scroller));
}

TEST_F(SpatialNavigationTest, StartAtVisibleFocusedIframe) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  iframe {"
      "    width: 100px;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<div>some text here</div>");

  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), iframe,
                         SpatialNavigationDirection::kDown),
            NodeRectInRootFrame(iframe));
}

TEST_F(SpatialNavigationTest, StartAtTopWhenGoingDownwardsWithoutFocus) {
  EXPECT_EQ(PhysicalRect(0, -1, 111, 0),
            SearchOrigin({0, 0, 111, 222}, nullptr,
                         SpatialNavigationDirection::kDown));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr,
                         SpatialNavigationDirection::kDown),
            TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtBottomWhenGoingUpwardsWithoutFocus) {
  EXPECT_EQ(
      PhysicalRect(0, 222 + 1, 111, 0),
      SearchOrigin({0, 0, 111, 222}, nullptr, SpatialNavigationDirection::kUp));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr,
                         SpatialNavigationDirection::kUp),
            BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtLeftSideWhenGoingEastWithoutFocus) {
  EXPECT_EQ(PhysicalRect(-1, 0, 0, 222),
            SearchOrigin({0, 0, 111, 222}, nullptr,
                         SpatialNavigationDirection::kRight));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr,
                         SpatialNavigationDirection::kRight),
            LeftSideOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtRightSideWhenGoingWestWithoutFocus) {
  EXPECT_EQ(PhysicalRect(111 + 1, 0, 0, 222),
            SearchOrigin({0, 0, 111, 222}, nullptr,
                         SpatialNavigationDirection::kLeft));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr,
                         SpatialNavigationDirection::kLeft),
            RightSideOfVisualViewport());
}

TEST_F(SpatialNavigationTest,
       StartAtBottomWhenGoingUpwardsAndFocusIsOffscreen) {
  SetBodyInnerHTML(
      "<button id='b' style='margin-top: 120%;'>B</button>");  // Outside the
                                                               // visual
                                                               // viewport.
  Element* b = GetDocument().getElementById(AtomicString("b"));
  EXPECT_TRUE(IsOffscreen(b));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kUp),
            BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtContainersEdge) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "    height: 100px;"
      "    width: 100px;"
      "    overflow: scroll;"
      "  }"
      "  button {"
      "    margin-top: 200px;"  // Outside the div's viewport.
      "  }"
      "</style>"
      "<div id='container'>"
      "  <button id='b'>B</button>"
      "</div>");

  Element* b = GetDocument().getElementById(AtomicString("b"));
  const Element* container =
      GetDocument().getElementById(AtomicString("container"));
  const PhysicalRect container_box = NodeRectInRootFrame(container);

  // TODO(crbug.com/889840):
  // VisibleBoundsInLocalRoot does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInLocalRoot, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(b->VisibleBoundsInLocalRoot().IsEmpty());  // EXPECT_TRUE.
  EXPECT_TRUE(IsOffscreen(b));

  // Go down.
  PhysicalRect container_top_edge = container_box;
  container_top_edge.SetHeight(LayoutUnit(0));
  container_top_edge.SetY(container_top_edge.Y() - 1);
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kDown),
            container_top_edge);

  // Go up.
  PhysicalRect container_bottom_edge = container_box;
  container_bottom_edge.SetHeight(LayoutUnit(0));
  container_bottom_edge.SetY(container_bottom_edge.Right() + 1);
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kUp),
            container_bottom_edge);

  // Go right.
  PhysicalRect container_leftmost_edge = container_box;
  container_leftmost_edge.SetWidth(LayoutUnit(0));
  container_leftmost_edge.SetX(container_leftmost_edge.X() - 1);
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kRight),
            container_leftmost_edge);

  // Go left.
  PhysicalRect container_rightmost_edge = container_box;
  container_rightmost_edge.SetX(container_bottom_edge.Right() + 1);
  container_rightmost_edge.SetWidth(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kLeft),
            container_rightmost_edge);
}

TEST_F(SpatialNavigationTest,
       StartFromDocEdgeWhenFocusIsClippedInOffscreenScroller) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "    margin-top: 120%;"  // Outside the visual viewport.
      "    height: 100px;"
      "    width: 100px;"
      "    overflow: scroll;"
      "  }"
      "  button {"
      "    margin-top: 300px;"  // Outside the div's scrollport.
      "  }"
      "</style>"
      "<div id='scroller'>"
      "  <button id='b'>B</button>"
      "</div>");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  Element* b = GetDocument().getElementById(AtomicString("b"));

  EXPECT_TRUE(IsOffscreen(scroller));
  EXPECT_TRUE(IsOffscreen(b));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kUp),
            BottomOfVisualViewport());
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kDown),
            TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest,
       StartFromDocEdgeWhenFocusIsClippedInNestedOffscreenScroller) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "   margin-top: 120%;"  // Outside the visual viewport.
      "   height: 100px;"
      "   width: 100px;"
      "   overflow: scroll;"
      "}"
      "a {"
      "  display: block;"
      "  margin-top: 300px;"
      "}"
      "</style>"
      "<div id='scroller1'>"
      "  <div id='scroller2'>"
      "    <a id='link'>link</a>"
      "  </div>"
      "</div>");

  Element* scroller1 = GetDocument().getElementById(AtomicString("scroller1"));
  Element* scroller2 = GetDocument().getElementById(AtomicString("scroller2"));
  Element* link = GetDocument().getElementById(AtomicString("link"));

  EXPECT_TRUE(IsScrollableAreaOrDocument(scroller1));
  EXPECT_TRUE(IsScrollableAreaOrDocument(scroller2));
  EXPECT_TRUE(IsOffscreen(scroller1));
  EXPECT_TRUE(IsOffscreen(scroller1));
  EXPECT_TRUE(IsOffscreen(link));

  AssertUseSidesOfVisualViewport(link);
}

TEST_F(SpatialNavigationTest, PartiallyVisible) {
  // <button>'s bottom is clipped.
  SetBodyInnerHTML("<button id='b' style='height: 900px;'>B</button>");
  Element* b = GetDocument().getElementById(AtomicString("b"));

  EXPECT_FALSE(IsOffscreen(b));  // <button> is not completely offscreen.

  PhysicalRect button_in_root_frame = NodeRectInRootFrame(b);

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kUp),
            Intersection(button_in_root_frame, RootViewport(&GetFrame())));

  // Do some scrolling.
  ScrollableArea* root_scroller = GetDocument().View()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(0, 600),
                                 mojom::blink::ScrollType::kProgrammatic);
  PhysicalRect button_after_scroll = NodeRectInRootFrame(b);
  ASSERT_NE(button_in_root_frame,
            button_after_scroll);  // As we scrolled, the
                                   // <button>'s position in
                                   // the root frame changed.

  // <button>'s top is clipped.
  EXPECT_FALSE(IsOffscreen(b));  // <button> is not completely offscreen.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kUp),
            Intersection(button_after_scroll, RootViewport(&GetFrame())));
}

TEST_F(SpatialNavigationTest,
       StartFromDocEdgeWhenOffscreenIframeDisplaysFocus) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  iframe {"
      "    margin-top: 120%;"  // Outside the visual viewport.
      "    height: 100px;"
      "    width: 100px;"
      "  }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<a id='link'>link</a>");

  UpdateAllLifecyclePhasesForTest();
  Element* link = ChildDocument().QuerySelector(AtomicString("a"));
  Element* iframe = GetDocument().QuerySelector(AtomicString("iframe"));

  // The <iframe> is not displayed in the visual viewport. In other words, it is
  // being offscreen. And so is also its content, the <a>.
  EXPECT_TRUE(IsOffscreen(iframe));
  EXPECT_TRUE(IsOffscreen(&ChildDocument()));
  EXPECT_TRUE(IsOffscreen(link));

  AssertUseSidesOfVisualViewport(link);
}

TEST_F(SpatialNavigationTest, DivsCanClipIframes) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {"
      "    height: 100px;"
      "    width: 100px;"
      "    overflow: scroll;"
      "  }"
      "  iframe {"
      "    margin-top: 200px;"  // Outside the div's viewport.
      "    height: 50px;"
      "    width: 50px;"
      "  }"
      "</style>"
      "<div>"
      "  <iframe id='iframe'></iframe>"
      "</div>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<a>link</a>");

  UpdateAllLifecyclePhasesForTest();
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Element* iframe = GetDocument().QuerySelector(AtomicString("iframe"));
  Element* link = ChildDocument().QuerySelector(AtomicString("a"));
  EXPECT_FALSE(IsOffscreen(div));

  // TODO(crbug.com/889840):
  // VisibleBoundsInLocalRoot does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInLocalRoot, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(iframe->VisibleBoundsInLocalRoot().IsEmpty());  // EXPECT_TRUE.

  // The <iframe> is not displayed in the visual viewport because it is clipped
  // by the div. In other words, it is being offscreen. And so is also its
  // content, the <a>.
  EXPECT_TRUE(IsOffscreen(iframe));
  EXPECT_TRUE(IsOffscreen(&ChildDocument()));
  EXPECT_TRUE(IsOffscreen(link));
}

TEST_F(SpatialNavigationTest, PartiallyVisibleIFrame) {
  // <a> is off screen. The <iframe> is visible, but partially off screen.
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  iframe {"
      "    width: 200%;"
      "    height: 100px;"
      "  }"
      "</style>"
      "<iframe id='iframe'></iframe>");

  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #child {"
      "    margin-left: 120%;"
      "  }"
      "</style>"
      "<a id='child'>link</a>");

  UpdateAllLifecyclePhasesForTest();
  Element* child_element =
      ChildDocument().getElementById(AtomicString("child"));
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);
  EXPECT_EQ(enclosing_container, ChildDocument());

  EXPECT_TRUE(IsOffscreen(child_element));         // Completely offscreen.
  EXPECT_FALSE(IsOffscreen(enclosing_container));  // Partially visible.

  PhysicalRect iframe = NodeRectInRootFrame(enclosing_container);

  // When searching downwards we start at activeElement's
  // container's (here: the iframe's) topmost visible edge.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), child_element,
                         SpatialNavigationDirection::kDown),
            OppositeEdge(SpatialNavigationDirection::kDown,
                         Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching upwards we start at activeElement's
  // container's (here: the iframe's) bottommost visible edge.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), child_element,
                         SpatialNavigationDirection::kUp),
            OppositeEdge(SpatialNavigationDirection::kUp,
                         Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching eastwards, "to the right", we start at activeElement's
  // container's (here: the iframe's) leftmost visible edge.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), child_element,
                         SpatialNavigationDirection::kRight),
            OppositeEdge(SpatialNavigationDirection::kRight,
                         Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching westwards, "to the left", we start at activeElement's
  // container's (here: the iframe's) rightmost visible edge.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), child_element,
                         SpatialNavigationDirection::kLeft),
            OppositeEdge(SpatialNavigationDirection::kLeft,
                         Intersection(iframe, RootViewport(&GetFrame()))));
}

TEST_F(SpatialNavigationTest, BottomOfPinchedViewport) {
  PhysicalRect origin = SearchOrigin(RootViewport(&GetFrame()), nullptr,
                                     SpatialNavigationDirection::kUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_EQ(origin.Width(), GetFrame().View()->Width());
  EXPECT_EQ(origin.X(), 0);
  EXPECT_EQ(origin.Y(), GetFrame().View()->Height() + 1);
  EXPECT_EQ(origin, BottomOfVisualViewport());

  // Now, test SearchOrigin with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(gfx::PointF(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr,
                        SpatialNavigationDirection::kUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_LT(origin.Y(), GetFrame().View()->Height() + 1);
  EXPECT_EQ(origin, BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StraightTextNoFragments) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  body {font: 10px/10px Ahem; width: 500px}"
      "</style>"
      "<a href='#' id='a'>blaaaaa blaaaaa blaaaaa</a>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  EXPECT_FALSE(IsFragmentedInline(*a->GetLayoutObject()));
}

TEST_F(SpatialNavigationTest, LineBrokenTextHasFragments) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  body {font: 10px/10px Ahem; width: 40px}"
      "</style>"
      "<a href='#' id='a'>blaaaaa blaaaaa blaaaaa</a>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  EXPECT_TRUE(IsFragmentedInline(*a->GetLayoutObject()));
}

TEST_F(SpatialNavigationTest, ManyClientRectsButNotLineBrokenText) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  div {width: 20px; height: 20px;}"
      "</style>"
      "<a href='#' id='a'><div></div></a>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  EXPECT_FALSE(IsFragmentedInline(*a->GetLayoutObject()));
}

TEST_F(SpatialNavigationTest, UseTheFirstFragment) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  body {font: 10px/10px Ahem; margin: 0; width: 50px;}"
      "</style>"
      "<a href='#' id='a'>12345 12</a>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  EXPECT_TRUE(IsFragmentedInline(*a->GetLayoutObject()));

  // Search downards.
  PhysicalRect origin_down = SearchOrigin(RootViewport(&GetFrame()), a,
                                          SpatialNavigationDirection::kDown);
  PhysicalRect origin_fragment =
      SearchOriginFragment(NodeRectInRootFrame(a), *a->GetLayoutObject(),
                           SpatialNavigationDirection::kDown);
  EXPECT_EQ(origin_down, origin_fragment);
  EXPECT_EQ(origin_down.Height(), 10);
  EXPECT_EQ(origin_down.Width(), 50);
  EXPECT_EQ(origin_down.X(), 0);
  EXPECT_EQ(origin_down.Y(), 0);

  // Search upwards.
  PhysicalRect origin_up = SearchOrigin(RootViewport(&GetFrame()), a,
                                        SpatialNavigationDirection::kUp);
  PhysicalRect origin_fragment_up =
      SearchOriginFragment(NodeRectInRootFrame(a), *a->GetLayoutObject(),
                           SpatialNavigationDirection::kUp);
  EXPECT_EQ(origin_up, origin_fragment_up);
  EXPECT_EQ(origin_up.Height(), 10);
  EXPECT_EQ(origin_up.Width(), 20);
  EXPECT_EQ(origin_up.X(), 0);
  EXPECT_EQ(origin_up.Y(), 10);

  // Search from the top fragment.
  PhysicalRect origin_left = SearchOrigin(RootViewport(&GetFrame()), a,
                                          SpatialNavigationDirection::kLeft);
  EXPECT_EQ(origin_left, origin_down);

  // Search from the bottom fragment.
  PhysicalRect origin_right = SearchOrigin(RootViewport(&GetFrame()), a,
                                           SpatialNavigationDirection::kRight);
  EXPECT_EQ(origin_right, origin_up);
}

TEST_F(SpatialNavigationTest, InlineImageLink) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<body style='font: 17px Ahem;'>"
      "<a id='a'><img id='pic' width='50' height='50'></a>"
      "</body>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  PhysicalRect uncropped_link = NodeRectInRootFrame(a);
  EXPECT_EQ(uncropped_link.Width(), 50);
  EXPECT_EQ(uncropped_link.Height(), 50);

  // The link gets its img's dimensions.
  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), a,
                                            SpatialNavigationDirection::kDown);
  EXPECT_EQ(search_origin, uncropped_link);
}

TEST_F(SpatialNavigationTest, InlineImageLinkWithLineHeight) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<body style='font: 17px Ahem; line-height: 13px;'>"
      "<a id='a'><img id='pic' width='50' height='50'></a>"
      "</body>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  PhysicalRect uncropped_link = NodeRectInRootFrame(a);
  EXPECT_EQ(uncropped_link.Width(), 50);
  EXPECT_EQ(uncropped_link.Height(), 50);

  // The link gets its img's dimensions.
  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), a,
                                            SpatialNavigationDirection::kDown);
  EXPECT_EQ(search_origin, uncropped_link);
}

TEST_F(SpatialNavigationTest, InlineImageTextLinkWithLineHeight) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 16px Ahem; line-height: 13px;'>"
      "<a id='a'><img width='30' height='30' id='replacedinline'>aaa</a> "
      "<a id='b'>b</a><br/>"
      "<a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));

  // The link gets its img's height.
  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), a,
                                            SpatialNavigationDirection::kDown);
  EXPECT_EQ(search_origin.Height(), 30);

  EXPECT_FALSE(Intersects(a, c));
  EXPECT_FALSE(Intersects(b, c));
}

TEST_F(SpatialNavigationTest, InlineLinkWithInnerBlock) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 20px Ahem; line-height: 16px;'>"
      "<a id='a'>a<span style='display: inline-block; width: 40px; height: "
      "45px; color: red'>a</span>a</a><a id='b'>bbb</a><br/>"
      "<a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));

  // The link gets its inner block's height.
  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), a,
                                            SpatialNavigationDirection::kDown);
  EXPECT_EQ(search_origin.Height(), 45);

  EXPECT_FALSE(Intersects(a, c));
  EXPECT_FALSE(Intersects(b, c));
}

TEST_F(SpatialNavigationTest, NoOverlappingLinks) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 17px Ahem;'>"
      "  <a id='a'>aaa</a> <a id='b'>bbb</a><br/>"
      "  <a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  AssertNormalizedHeight(a, 17, false);
  AssertNormalizedHeight(b, 17, false);
  AssertNormalizedHeight(c, 17, false);
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));
  EXPECT_FALSE(Intersects(a, b));
  EXPECT_FALSE(Intersects(a, c));
}

TEST_F(SpatialNavigationTest, OverlappingLinks) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 16px Ahem; line-height: 13px;'>"
      "  <a id='a'>aaa</a> <a id='b'>bbb</a><br/>"
      "  <a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  // SpatNav will use the line box's height.
  AssertNormalizedHeight(a, 13, true);
  AssertNormalizedHeight(b, 13, true);
  AssertNormalizedHeight(c, 13, true);
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));
  EXPECT_FALSE(Intersects(a, b));
  EXPECT_FALSE(Intersects(a, c));
}

TEST_F(SpatialNavigationTest, UseInlineBoxHeightWhenShorter) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 17px Ahem; line-height: 20px'>"
      "  <a id='a'>aaa</a> <a id='b'>bbb</a><br/>"
      "  <a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  // SpatNav will use the inline boxes' height (17px) when it's shorter than
  // their line box (20px).
  AssertNormalizedHeight(a, 17, false);
  AssertNormalizedHeight(b, 17, false);
  AssertNormalizedHeight(c, 17, false);
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));
  EXPECT_FALSE(Intersects(a, b));
  EXPECT_FALSE(Intersects(a, c));
}

TEST_F(SpatialNavigationTest, LineBrokenLink) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  body {font: 10px Ahem; line-height: 12px; width: 40px}"
      "</style>"
      "<a id='a'>bla bla bla</a>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  ASSERT_TRUE(IsFragmentedInline(*a->GetLayoutObject()));
  ASSERT_EQ(LineBoxes(*a->GetLayoutObject()), 3);
  PhysicalRect search_origin =
      SearchOrigin(RootViewport(a->GetDocument().GetFrame()), a,
                   SpatialNavigationDirection::kDown);
  // The line box (12px) is bigger than the inline box (10px).
  EXPECT_EQ(search_origin.Height(), 10);

  // A line broken link's search origin will only be the first or last line box.
  // The candidate rect will still contain all line boxes.
  EXPECT_FALSE(HasSameSearchOriginRectAndCandidateRect(a));

  FocusCandidate candidate(a, SpatialNavigationDirection::kDown);
  PhysicalRect uncropped = NodeRectInRootFrame(a);
  EXPECT_EQ(uncropped, candidate.rect_in_root_frame);
  EXPECT_EQ(candidate.rect_in_root_frame.Height(), 12 + 12 + 10);
}

TEST_F(SpatialNavigationTest, NormalizedLineBrokenLink) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  body {font: 10px Ahem; line-height: 7px; width: 40px}"
      "</style>"
      "<a id='a'>bla bla bla</a>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  ASSERT_TRUE(IsFragmentedInline(*a->GetLayoutObject()));
  ASSERT_EQ(LineBoxes(*a->GetLayoutObject()), 3);
  PhysicalRect search_origin =
      SearchOrigin(RootViewport(a->GetDocument().GetFrame()), a,
                   SpatialNavigationDirection::kDown);
  // The line box (7px) is smaller than the inline box (10px).
  EXPECT_EQ(search_origin.Height(), 7);

  // A line broken link's search origin will only be the first or last line box.
  // The candidate rect will still contain all line boxes.
  EXPECT_FALSE(HasSameSearchOriginRectAndCandidateRect(a));

  FocusCandidate candidate(a, SpatialNavigationDirection::kDown);
  PhysicalRect uncropped = NodeRectInRootFrame(a);
  EXPECT_LT(candidate.rect_in_root_frame.Height(), uncropped.Height());
  EXPECT_EQ(candidate.rect_in_root_frame.Height(), 3 * 7);
}

TEST_F(SpatialNavigationTest, NormalizedLineBrokenLinkWithImg) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "body {font: 10px Ahem; line-height: 7px;}"
      "</style>"
      "<div style='width: 40px'>"
      "<a id='a'>aa<img width='10' height='24' src=''>a aaaa</a>"
      "<a id='b'>bb</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  ASSERT_TRUE(IsFragmentedInline(*a->GetLayoutObject()));
  ASSERT_FALSE(IsFragmentedInline(*b->GetLayoutObject()));
  ASSERT_EQ(LineBoxes(*a->GetLayoutObject()), 2);
  ASSERT_EQ(LineBoxes(*b->GetLayoutObject()), 1);

  // A line broken link's search origin will only be the first or last line box.
  // The candidate rect will still contain all line boxes.
  EXPECT_FALSE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_FALSE(Intersects(a, b));
}

TEST_F(SpatialNavigationTest, PaddedInlineLinkOverlapping) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 18px Ahem; line-height: 13px;'>"
      "  <a id='a' style='padding: 10px;'>aaa</a>"
      "  <a id='b'>bbb</a><br/>"
      "  <a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  // Padding doesn't grow |a|'s line box.
  AssertNormalizedHeight(a, 13, true);
  AssertNormalizedHeight(b, 13, true);
  AssertNormalizedHeight(c, 13, true);
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));
  EXPECT_FALSE(Intersects(a, b));
  EXPECT_FALSE(Intersects(a, c));
}

TEST_F(SpatialNavigationTest, PaddedInlineBlockLinkOverlapping) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 18px Ahem; line-height: 13px;'>"
      "  <a id='a' style='display: inline-block; padding: 10px;'>aaa</a>"
      "  <a id='b'>bbb</a><br/>"
      "  <a id='c'>cccccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));
  EXPECT_FALSE(Intersects(a, b));
  EXPECT_FALSE(Intersects(a, c));
}

TEST_F(SpatialNavigationTest, BoxWithLineHeight) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 16px Ahem; line-height: 13px;' id='block'>"
      "  aaa bbb<br/>"
      "  <a id='c'>cccccccc</a>"
      "</div>");
  Element* block = GetDocument().getElementById(AtomicString("block"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  ASSERT_TRUE(Intersects(block, c));

  // The block's inner line-height does not change the block's outer dimensions.
  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), block,
                                            SpatialNavigationDirection::kDown);
  PhysicalRect uncropped = NodeRectInRootFrame(block);
  PhysicalRect normalized =
      ShrinkInlineBoxToLineBox(*block->GetLayoutObject(), uncropped);
  EXPECT_EQ(search_origin, uncropped);
  EXPECT_EQ(normalized, uncropped);
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(block));
}

TEST_F(SpatialNavigationTest, ReplacedInlineElement) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<body style='font: 16px Ahem; line-height: 13px;'>"
      "  <img width='20' height='20' id='pic'> bbb<br/>"
      "  <a id='c'>cccccccc</a>"
      "</body>");
  Element* pic = GetDocument().getElementById(AtomicString("pic"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  EXPECT_FALSE(Intersects(pic, c));

  // The line-height around the img does not change the img's outer dimensions.
  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), pic,
                                            SpatialNavigationDirection::kDown);
  PhysicalRect uncropped = NodeRectInRootFrame(pic);
  PhysicalRect normalized =
      ShrinkInlineBoxToLineBox(*pic->GetLayoutObject(), uncropped);
  EXPECT_EQ(search_origin, uncropped);
  EXPECT_EQ(normalized, uncropped);
  EXPECT_EQ(search_origin.Width(), 20);
  EXPECT_EQ(search_origin.Height(), 20);
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(pic));
}

TEST_F(SpatialNavigationTest, VerticalText) {
  LoadAhem();
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<div style='font: 14px/14px Ahem; line-height: 12px; writing-mode: "
      "vertical-lr; height: 160px'>"
      "<a id='a'>aaaaaaaaaaa</a>"
      "<a id='b'>bbb</a> <a id='c'>cccccc</a>"
      "</div>");
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(a));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(b));
  EXPECT_TRUE(HasSameSearchOriginRectAndCandidateRect(c));
  EXPECT_FALSE(Intersects(a, b));
  EXPECT_FALSE(Intersects(a, c));

  PhysicalRect search_origin = SearchOrigin(RootViewport(&GetFrame()), a,
                                            SpatialNavigationDirection::kDown);
  ASSERT_EQ(search_origin.Height(), 14 * 11);
  EXPECT_EQ(search_origin.Width(), 12);  // The logical line-height.
}

TEST_F(SpatialNavigationTest, TopOfPinchedViewport) {
  PhysicalRect origin = SearchOrigin(RootViewport(&GetFrame()), nullptr,
                                     SpatialNavigationDirection::kDown);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_EQ(origin.Width(), GetFrame().View()->Width());
  EXPECT_EQ(origin.X(), 0);
  EXPECT_EQ(origin.Y(), -1);
  EXPECT_EQ(origin, TopOfVisualViewport());

  // Now, test SearchOrigin with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(gfx::PointF(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr,
                        SpatialNavigationDirection::kDown);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_GT(origin.Y(), -1);
  EXPECT_EQ(origin, TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest, HasRemoteFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad("about:blank");

  WebViewImpl* webview = helper.GetWebView();
  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(webview->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<iframe id='iframe'></iframe>",
                                     base_url);

  webview->ResizeWithBrowserControls(gfx::Size(400, 400), 50, 0, false);
  UpdateAllLifecyclePhasesForTest();

  Element* iframe =
      webview->MainFrameImpl()->GetFrame()->GetDocument()->getElementById(
          AtomicString("iframe"));
  EXPECT_FALSE(HasRemoteFrame(iframe));

  frame_test_helpers::SwapRemoteFrame(webview->MainFrameImpl()->FirstChild(),
                                      frame_test_helpers::CreateRemote());
  EXPECT_TRUE(HasRemoteFrame(iframe));
}

TEST_F(SpatialNavigationTest, PressEnterKeyActiveElement) {
  SetBodyInnerHTML("<button id='b'>hello</button>");

  Element* b = GetDocument().getElementById(AtomicString("b"));

  // Move interest to button.
  WebKeyboardEvent arrow_down{WebInputEvent::Type::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests()};
  arrow_down.dom_key = ui::DomKey::ARROW_DOWN;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(arrow_down);

  arrow_down.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(arrow_down);

  EXPECT_FALSE(b->IsActive());

  // Enter key down add :active state to element.
  WebKeyboardEvent enter{WebInputEvent::Type::kRawKeyDown,
                         WebInputEvent::kNoModifiers,
                         WebInputEvent::GetStaticTimeStampForTests()};
  enter.dom_key = ui::DomKey::ENTER;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(enter);
  EXPECT_TRUE(b->IsActive());

  // Enter key up remove :active state to element.
  enter.SetType(WebInputEvent::Type::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(enter);
  EXPECT_FALSE(b->IsActive());
}

}  // namespace blink
