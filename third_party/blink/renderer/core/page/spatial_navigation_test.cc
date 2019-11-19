// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

class SpatialNavigationTest : public RenderingTest {
 public:
  SpatialNavigationTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

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

  void UpdateAllLifecyclePhases(LocalFrameView* frame_view) {
    frame_view->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }
};

TEST_F(SpatialNavigationTest, RootFramesVisualViewport) {
  // Test RootViewport with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));

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

  Element* child_element = GetDocument().getElementById("child");
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

  UpdateAllLifecyclePhases(ChildDocument().View());
  Element* iframe = GetDocument().QuerySelector("iframe");
  Element* link = ChildDocument().QuerySelector("a");
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

  Element* content = GetDocument().getElementById("content");
  Element* container = GetDocument().getElementById("container");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(content);

  // TODO(crbug.com/889840):
  // VisibleBoundsInVisualViewport does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInVisualViewport, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(
      content->VisibleBoundsInVisualViewport().IsEmpty());  // EXPECT_TRUE.

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

  Element* a = GetDocument().getElementById("a");
  Element* b = GetDocument().getElementById("b");
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
  Element* b = GetDocument().getElementById("b");

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

  Element* scroller = GetDocument().getElementById("scroller");
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

  Element* iframe = GetDocument().getElementById("iframe");
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
  Element* b = GetDocument().getElementById("b");
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

  Element* b = GetDocument().getElementById("b");
  const Element* container = GetDocument().getElementById("container");
  const PhysicalRect container_box = NodeRectInRootFrame(container);

  // TODO(crbug.com/889840):
  // VisibleBoundsInVisualViewport does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInVisualViewport, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(b->VisibleBoundsInVisualViewport().IsEmpty());  // EXPECT_TRUE.
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

  Element* scroller = GetDocument().getElementById("scroller");
  Element* b = GetDocument().getElementById("b");

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

  Element* scroller1 = GetDocument().getElementById("scroller1");
  Element* scroller2 = GetDocument().getElementById("scroller2");
  Element* link = GetDocument().getElementById("link");

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
  Element* b = GetDocument().getElementById("b");

  EXPECT_FALSE(IsOffscreen(b));  // <button> is not completely offscreen.

  PhysicalRect button_in_root_frame = NodeRectInRootFrame(b);

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b,
                         SpatialNavigationDirection::kUp),
            Intersection(button_in_root_frame, RootViewport(&GetFrame())));

  // Do some scrolling.
  ScrollableArea* root_scroller = GetDocument().View()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(0, 600), kProgrammaticScroll);
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

  UpdateAllLifecyclePhases(ChildDocument().View());
  Element* link = ChildDocument().QuerySelector("a");
  Element* iframe = GetDocument().QuerySelector("iframe");

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

  UpdateAllLifecyclePhases(ChildDocument().View());
  Element* div = GetDocument().QuerySelector("div");
  Element* iframe = GetDocument().QuerySelector("iframe");
  Element* link = ChildDocument().QuerySelector("a");
  EXPECT_FALSE(IsOffscreen(div));

  // TODO(crbug.com/889840):
  // VisibleBoundsInVisualViewport does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInVisualViewport, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(
      iframe->VisibleBoundsInVisualViewport().IsEmpty());  // EXPECT_TRUE.

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

  UpdateAllLifecyclePhases(ChildDocument().View());
  Element* child_element = ChildDocument().getElementById("child");
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
  visual_viewport.SetLocation(FloatPoint(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr,
                        SpatialNavigationDirection::kUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_LT(origin.Y(), GetFrame().View()->Height() + 1);
  EXPECT_EQ(origin, BottomOfVisualViewport());
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
  visual_viewport.SetLocation(FloatPoint(200, 200));
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

  webview->ResizeWithBrowserControls(IntSize(400, 400), 50, 0, false);
  UpdateAllLifecyclePhases(webview->MainFrameImpl()->GetFrame()->View());

  Element* iframe =
      webview->MainFrameImpl()->GetFrame()->GetDocument()->getElementById(
          "iframe");
  EXPECT_FALSE(HasRemoteFrame(iframe));

  webview->MainFrameImpl()->FirstChild()->Swap(
      frame_test_helpers::CreateRemote());
  EXPECT_TRUE(HasRemoteFrame(iframe));
}

class SpatialNavigationWithFocuslessModeTest
    : public SpatialNavigationTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SpatialNavigationWithFocuslessModeTest() : use_focusless_mode_(GetParam()) {}

  void SetUp() override {
    SpatialNavigationTest::SetUp();
    GetDocument().GetSettings()->SetSpatialNavigationEnabled(true);
  }

 private:
  ScopedFocuslessSpatialNavigationForTest use_focusless_mode_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SpatialNavigationWithFocuslessModeTest,
                         ::testing::Bool());

TEST_P(SpatialNavigationWithFocuslessModeTest, PressEnterKeyActiveElement) {
  SetBodyInnerHTML("<button id='b'>hello</button>");

  Element* b = GetDocument().getElementById("b");

  // Move interest to button.
  WebKeyboardEvent arrow_down{WebInputEvent::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests()};
  arrow_down.dom_key = ui::DomKey::ARROW_DOWN;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(arrow_down);

  arrow_down.SetType(WebInputEvent::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(arrow_down);

  EXPECT_FALSE(b->IsActive());

  // Enter key down add :active state to element.
  WebKeyboardEvent enter{WebInputEvent::kRawKeyDown,
                         WebInputEvent::kNoModifiers,
                         WebInputEvent::GetStaticTimeStampForTests()};
  enter.dom_key = ui::DomKey::ENTER;
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(enter);
  EXPECT_TRUE(b->IsActive());

  // Enter key up remove :active state to element.
  enter.SetType(WebInputEvent::kKeyUp);
  GetDocument().GetFrame()->GetEventHandler().KeyEvent(enter);
  EXPECT_FALSE(b->IsActive());
}

class FocuslessSpatialNavigationSimTest : public SimTest {
 public:
  FocuslessSpatialNavigationSimTest() : use_focusless_mode_(true) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetPage()->GetSettings().SetSpatialNavigationEnabled(true);
  }

  void SimulateKeyPress(int dom_key) {
    WebKeyboardEvent event{WebInputEvent::kRawKeyDown,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests()};
    event.dom_key = dom_key;
    WebView().MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(event));

    if (dom_key == ui::DomKey::ENTER) {
      event.SetType(WebInputEvent::kChar);
      WebView().MainFrameWidget()->HandleInputEvent(
          WebCoalescedInputEvent(event));
    }

    event.SetType(WebInputEvent::kKeyUp);
    WebView().MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(event));
  }

  ScopedFocuslessSpatialNavigationForTest use_focusless_mode_;
};

// Tests that opening a <select> popup works by pressing enter from
// "interested" mode, without being focused.
TEST_F(FocuslessSpatialNavigationSimTest, OpenSelectPopup) {
  // This test requires PagePopup since we're testing opening the <select> drop
  // down so skip this test on platforms (i.e. Android) that don't use this.
  if (!RuntimeEnabledFeatures::PagePopupEnabled())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  WebView().MainFrameWidget()->SetFocus(true);
  WebView().SetIsActive(true);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <select id="target">
            <option>A</option>
            <option>B</option>
            <option>C</option>
          </select>
      )HTML");
  Compositor().BeginFrame();

  auto* select = To<HTMLSelectElement>(GetDocument().getElementById("target"));
  SimulateKeyPress(ui::DomKey::ARROW_DOWN);

  SpatialNavigationController& spat_nav_controller =
      GetDocument().GetPage()->GetSpatialNavigationController();

  ASSERT_EQ(select, spat_nav_controller.GetInterestedElement());
  ASSERT_NE(select, GetDocument().ActiveElement());
  ASSERT_FALSE(select->PopupIsVisible());

  // The enter key should cause the popup to open.
  SimulateKeyPress(ui::DomKey::ENTER);
  EXPECT_TRUE(select->PopupIsVisible());
}

}  // namespace blink
