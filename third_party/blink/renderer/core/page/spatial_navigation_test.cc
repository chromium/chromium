// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/spatial_navigation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class SpatialNavigationTest : public RenderingTest {
 public:
  SpatialNavigationTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

  LayoutRect TopOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetHeight(LayoutUnit(0));
    return visual_viewport;
  }

  LayoutRect BottomOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetY(visual_viewport.MaxY());
    visual_viewport.SetHeight(LayoutUnit(0));
    return visual_viewport;
  }

  LayoutRect LeftSideOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetWidth(LayoutUnit(0));
    return visual_viewport;
  }

  LayoutRect RightSideOfVisualViewport() {
    LayoutRect visual_viewport = RootViewport(&GetFrame());
    visual_viewport.SetX(visual_viewport.MaxX());
    visual_viewport.SetWidth(LayoutUnit(0));
    return visual_viewport;
  }

  void AssertUseSidesOfVisualViewport(Node* focus_node) {
    EXPECT_EQ(
        SearchOrigin(RootViewport(&GetFrame()), focus_node, kWebFocusTypeUp),
        BottomOfVisualViewport());
    EXPECT_EQ(
        SearchOrigin(RootViewport(&GetFrame()), focus_node, kWebFocusTypeDown),
        TopOfVisualViewport());
    EXPECT_EQ(
        SearchOrigin(RootViewport(&GetFrame()), focus_node, kWebFocusTypeLeft),
        RightSideOfVisualViewport());
    EXPECT_EQ(
        SearchOrigin(RootViewport(&GetFrame()), focus_node, kWebFocusTypeRight),
        LeftSideOfVisualViewport());
  }
};

TEST_F(SpatialNavigationTest, RootFramesVisualViewport) {
  // Test RootViewport with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));

  LocalFrameView* root_frame_view = GetFrame().LocalFrameRoot().View();
  const LayoutRect roots_visible_doc_rect(
      root_frame_view->GetScrollableArea()->VisibleContentRect());
  // Convert the root frame's visible rect from document space -> frame space.
  // For the root frame, frame space == root frame space, obviously.
  LayoutRect viewport_rect_of_root_frame =
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

  ChildDocument().View()->UpdateAllLifecyclePhases();
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
  EXPECT_EQ(RootViewport(&GetFrame()), LayoutRect(0, 0, 800, 600));

  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetSize({123, 123});
  EXPECT_EQ(RootViewport(&GetFrame()), LayoutRect(0, 0, 123, 123));
}

TEST_F(SpatialNavigationTest, StartAtVisibleFocusedElement) {
  SetBodyInnerHTML("<button id='b'>hello</button>");
  Element* b = GetDocument().getElementById("b");

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeDown),
            NodeRectInRootFrame(b, true));
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
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), scroller, kWebFocusTypeDown),
      NodeRectInRootFrame(scroller, true));
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
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), iframe, kWebFocusTypeDown),
            NodeRectInRootFrame(iframe, true));
}

TEST_F(SpatialNavigationTest, StartAtTopWhenGoingDownwardsWithoutFocus) {
  EXPECT_EQ(LayoutRect(0, 0, 111, 0),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeDown));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeDown),
            TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtBottomWhenGoingUpwardsWithoutFocus) {
  EXPECT_EQ(LayoutRect(0, 222, 111, 0),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeUp));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeUp),
            BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtLeftSideWhenGoingEastWithoutFocus) {
  EXPECT_EQ(LayoutRect(0, 0, 0, 222),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeRight));

  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeRight),
      LeftSideOfVisualViewport());
}

TEST_F(SpatialNavigationTest, StartAtRightSideWhenGoingWestWithoutFocus) {
  EXPECT_EQ(LayoutRect(111, 0, 0, 222),
            SearchOrigin({0, 0, 111, 222}, nullptr, kWebFocusTypeLeft));

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeLeft),
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

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
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
  const LayoutRect container_box = NodeRectInRootFrame(container, true);

  // TODO(crbug.com/889840):
  // VisibleBoundsInVisualViewport does not (yet) take div-clipping into
  // account. The node is off screen, but nevertheless VBIVV returns a non-
  // empty rect. If you fix VisibleBoundsInVisualViewport, change to
  // EXPECT_TRUE here and stop using LayoutObject in IsOffscreen().
  EXPECT_FALSE(b->VisibleBoundsInVisualViewport().IsEmpty());  // EXPECT_TRUE.
  EXPECT_TRUE(IsOffscreen(b));

  // Go down.
  LayoutRect container_top_edge = container_box;
  container_top_edge.SetHeight(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeDown),
            container_top_edge);

  // Go up.
  LayoutRect container_bottom_edge = container_box;
  container_bottom_edge.SetY(container_bottom_edge.MaxX());
  container_bottom_edge.SetHeight(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            container_bottom_edge);

  // Go right.
  LayoutRect container_leftmost_edge = container_box;
  container_leftmost_edge.SetWidth(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeRight),
            container_leftmost_edge);

  // Go left.
  LayoutRect container_rightmost_edge = container_box;
  container_rightmost_edge.SetX(container_bottom_edge.MaxX());
  container_rightmost_edge.SetWidth(LayoutUnit(0));
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeLeft),
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

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            BottomOfVisualViewport());
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeDown),
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

  LayoutRect button_in_root_frame = NodeRectInRootFrame(b, true);

  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
            Intersection(button_in_root_frame, RootViewport(&GetFrame())));

  // Do some scrolling.
  ScrollableArea* root_scroller = GetDocument().View()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(0, 600), kProgrammaticScroll);
  LayoutRect button_after_scroll = NodeRectInRootFrame(b, true);
  ASSERT_NE(button_in_root_frame,
            button_after_scroll);  // As we scrolled, the
                                   // <button>'s position in
                                   // the root frame changed.

  // <button>'s top is clipped.
  EXPECT_FALSE(IsOffscreen(b));  // <button> is not completely offscreen.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), b, kWebFocusTypeUp),
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

  ChildDocument().View()->UpdateAllLifecyclePhases();
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

  ChildDocument().View()->UpdateAllLifecyclePhases();
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

  ChildDocument().View()->UpdateAllLifecyclePhases();
  Element* child_element = ChildDocument().getElementById("child");
  Node* enclosing_container = ScrollableAreaOrDocumentOf(child_element);
  EXPECT_EQ(enclosing_container, ChildDocument());

  EXPECT_TRUE(IsOffscreen(child_element));         // Completely offscreen.
  EXPECT_FALSE(IsOffscreen(enclosing_container));  // Partially visible.

  LayoutRect iframe = NodeRectInRootFrame(enclosing_container, true);

  // When searching downwards we start at activeElement's
  // container's (here: the iframe's) topmost visible edge.
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), child_element, kWebFocusTypeDown),
      OppositeEdge(kWebFocusTypeDown,
                   Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching upwards we start at activeElement's
  // container's (here: the iframe's) bottommost visible edge.
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), child_element, kWebFocusTypeUp),
      OppositeEdge(kWebFocusTypeUp,
                   Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching eastwards, "to the right", we start at activeElement's
  // container's (here: the iframe's) leftmost visible edge.
  EXPECT_EQ(SearchOrigin(RootViewport(&GetFrame()), child_element,
                         kWebFocusTypeRight),
            OppositeEdge(kWebFocusTypeRight,
                         Intersection(iframe, RootViewport(&GetFrame()))));

  // When searching westwards, "to the left", we start at activeElement's
  // container's (here: the iframe's) rightmost visible edge.
  EXPECT_EQ(
      SearchOrigin(RootViewport(&GetFrame()), child_element, kWebFocusTypeLeft),
      OppositeEdge(kWebFocusTypeLeft,
                   Intersection(iframe, RootViewport(&GetFrame()))));
}

TEST_F(SpatialNavigationTest, BottomOfPinchedViewport) {
  LayoutRect origin =
      SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_EQ(origin.Width(), GetFrame().View()->Width());
  EXPECT_EQ(origin.X(), 0);
  EXPECT_EQ(origin.Y(), GetFrame().View()->Height());
  EXPECT_EQ(origin, BottomOfVisualViewport());

  // Now, test SearchOrigin with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeUp);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_LT(origin.Y(), GetFrame().View()->Height());
  EXPECT_EQ(origin, BottomOfVisualViewport());
}

TEST_F(SpatialNavigationTest, TopOfPinchedViewport) {
  LayoutRect origin =
      SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeDown);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_EQ(origin.Width(), GetFrame().View()->Width());
  EXPECT_EQ(origin.X(), 0);
  EXPECT_EQ(origin.Y(), 0);
  EXPECT_EQ(origin, TopOfVisualViewport());

  // Now, test SearchOrigin with a pinched viewport.
  VisualViewport& visual_viewport = GetFrame().GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));
  origin = SearchOrigin(RootViewport(&GetFrame()), nullptr, kWebFocusTypeDown);
  EXPECT_EQ(origin.Height(), 0);
  EXPECT_LT(origin.Width(), GetFrame().View()->Width());
  EXPECT_GT(origin.X(), 0);
  EXPECT_GT(origin.Y(), 0);
  EXPECT_EQ(origin, TopOfVisualViewport());
}

TEST_F(SpatialNavigationTest, HasRemoteFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad("about:blank", nullptr, nullptr, nullptr, nullptr);

  WebViewImpl* webview = helper.GetWebView();
  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(webview->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<iframe id='iframe'></iframe>",
                                     base_url);

  webview->ResizeWithBrowserControls(IntSize(400, 400), 50, 0, false);
  webview->MainFrameImpl()->GetFrame()->View()->UpdateAllLifecyclePhases();

  Element* iframe =
      webview->MainFrameImpl()->GetFrame()->GetDocument()->getElementById(
          "iframe");
  EXPECT_FALSE(HasRemoteFrame(iframe));

  webview->MainFrameImpl()->FirstChild()->Swap(
      frame_test_helpers::CreateRemote());
  EXPECT_TRUE(HasRemoteFrame(iframe));
}

}  // namespace blink
