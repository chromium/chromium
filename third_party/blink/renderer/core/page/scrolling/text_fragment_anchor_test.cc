// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentAnchorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }

  void RunAsyncMatchingTasks() {
    auto* scheduler =
        ThreadScheduler::Current()->GetWebMainThreadSchedulerForTest();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    RunPendingTasks();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  IntRect ViewportRect() {
    return IntRect(IntPoint(), LayoutViewport()->VisibleContentRect().Size());
  }

  IntRect BoundingRectInFrame(Node& node) {
    return node.GetLayoutObject()->AbsoluteBoundingBoxRect();
  }

  void SimulateClick(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  }

  void SimulateTap(int x, int y) {
    WebGestureEvent event(WebInputEvent::kGestureTap,
                          WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                          WebGestureDevice::kTouchscreen);
    event.SetPositionInWidget(FloatPoint(x, y));
    event.SetPositionInScreen(FloatPoint(x, y));
    event.SetFrameScale(1);
    GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(event);
  }
};

// Basic test case, ensure we scroll the matching text into view.
TEST_F(TextFragmentAnchorTest, BasicSmokeTest) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Make sure a non-matching string doesn't cause scroll and the fragment is
// removed when completed.
TEST_F(TextFragmentAnchorTest, NonMatchingString) {
  SimRequest request("https://example.com/test.html#:~:text=unicorn",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=unicorn");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());

  // Force a layout
  GetDocument().body()->setAttribute(html_names::kStyleAttr, "height: 1300px");
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_TRUE(GetDocument().Markers().Markers().IsEmpty());
}

// Ensure multiple matches will scroll the first into view.
TEST_F(TextFragmentAnchorTest, MultipleMatches) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(first)))
      << "First <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  // Ensure we only report one marker.
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure matching works inside nested blocks.
TEST_F(TextFragmentAnchorTest, NestedBlocks) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #spacer {
        height: 1000px;
      }
    </style>
    <body>
      <div id="spacer">
        Some non-matching text
      </div>
      <div>
        <p id="match">This is a test page</p>
      </div>
    </body>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& match = *GetDocument().getElementById("match");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(match)))
      << "<p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Ensure multiple texts are highlighted and the first is scrolled into
// view.
TEST_F(TextFragmentAnchorTest, MultipleTextFragments) {
  SimRequest request("https://example.com/test.html#:~:text=test&text=more",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test&text=more");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">This is some more text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& first = *GetDocument().getElementById("first");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(first)))
      << "First <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Ensure we scroll the second text into view if the first isn't found.
TEST_F(TextFragmentAnchorTest, FirstTextFragmentNotFound) {
  SimRequest request("https://example.com/test.html#:~:text=test&text=more",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test&text=more");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a page</p>
    <p id="second">This is some more text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& second = *GetDocument().getElementById("second");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(second)))
      << "Second <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure we still scroll the first text into view if the second isn't
// found.
TEST_F(TextFragmentAnchorTest, OnlyFirstTextFragmentFound) {
  SimRequest request("https://example.com/test.html#:~:text=test&text=more",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test&text=more");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Make sure multiple non-matching strings doesn't cause scroll and the fragment
// is removed when completed.
TEST_F(TextFragmentAnchorTest, MultipleNonMatchingStrings) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=unicorn&text=cookie&text=cat",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=unicorn&text=cookie&text=cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());

  // Force a layout
  GetDocument().body()->setAttribute(html_names::kStyleAttr, "height: 1300px");
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_TRUE(GetDocument().Markers().Markers().IsEmpty());
}

// Test matching a text range within the same element
TEST_F(TextFragmentAnchorTest, SameElementTextRange) {
  SimRequest request("https://example.com/test.html#:~:text=This,page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=This,page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "This is a test page".
  auto* text = To<Text>(GetDocument().getElementById("text")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());
}

// Test matching a text range across two neighboring elements
TEST_F(TextFragmentAnchorTest, NeighboringElementTextRange) {
  SimRequest request("https://example.com/test.html#:~:text=test,paragraph",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test,paragraph");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text1">This is a test page</p>
    <p id="text2">with another paragraph of text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  auto* text1 = To<Text>(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on "with another paragraph"
  auto* text2 = To<Text>(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(22u, markers.at(0)->EndOffset());
}

// Test matching a text range from an element to a deeper nested element
TEST_F(TextFragmentAnchorTest, DifferentDepthElementTextRange) {
  SimRequest request("https://example.com/test.html#:~:text=test,paragraph",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test,paragraph");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text1">This is a test page</p>
    <div>
      <p id="text2">with another paragraph of text</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  auto* text1 = To<Text>(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on "with another paragraph"
  auto* text2 = To<Text>(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(22u, markers.at(0)->EndOffset());
}

// Ensure that we don't match anything if endText is not found.
TEST_F(TextFragmentAnchorTest, TextRangeEndTextNotFound) {
  SimRequest request("https://example.com/test.html#:~:text=test,cat",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test,cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
}

// Test matching multiple text ranges
TEST_F(TextFragmentAnchorTest, MultipleTextRanges) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test,with&text=paragraph,text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test,with&text=paragraph,text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text1">This is a test page</p>
    <div>
      <p id="text2">with another paragraph of text</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  auto* text1 = To<Text>(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect markers on "with" and "paragraph of text"
  auto* text2 = To<Text>(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(2u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(4u, markers.at(0)->EndOffset());
  EXPECT_EQ(13u, markers.at(1)->StartOffset());
  EXPECT_EQ(30u, markers.at(1)->EndOffset());
}

// Ensure we scroll to the beginning of a text range larger than the viewport.
TEST_F(TextFragmentAnchorTest, DistantElementTextRange) {
  SimRequest request("https://example.com/test.html#:~:text=test,paragraph",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test,paragraph");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 3000px;
      }
    </style>
    <p id="text">This is a test page</p>
    <p>with another paragraph of text</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Test a text range with both context terms in the same element.
TEST_F(TextFragmentAnchorTest, TextRangeWithContext) {
  SimRequest request(
      "https://example.com/test.html#:~:text=This-,is,test,-page", "text/html");
  LoadURL("https://example.com/test.html#:~:text=This-,is,test,-page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "is a test".
  auto* text = To<Text>(GetDocument().getElementById("text")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(5u, markers.at(0)->StartOffset());
  EXPECT_EQ(14u, markers.at(0)->EndOffset());
}

// Ensure that we do not match a text range if the prefix is not found.
TEST_F(TextFragmentAnchorTest, PrefixNotFound) {
  SimRequest request(
      "https://example.com/test.html#:~:text=prefix-,is,test,-page",
      "text/html");
  LoadURL("https://example.com/test.html#:~:text=prefix-,is,test,-page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

// Ensure that we do not match a text range if the suffix is not found.
TEST_F(TextFragmentAnchorTest, SuffixNotFound) {
  SimRequest request(
      "https://example.com/test.html#:~:text=This-,is,test,-suffix",
      "text/html");
  LoadURL("https://example.com/test.html#:~:text=This-,is,test,-suffix");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

// Test a text range with context terms in different elements
TEST_F(TextFragmentAnchorTest, TextRangeWithCrossElementContext) {
  SimRequest request(
      "https://example.com/test.html#:~:text=Header%202-,A,text,-Footer%201",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=Header%202-,A,text,-Footer%201");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <h1>Header 1</h1>
    <p>A string of text</p>
    <p>Footer 1</p>
    <h1>Header 2</h1>
    <p id="expected">A string of text</p>
    <p>Footer 1</p>
    <h1>Header 2</h1>
    <p>A string of text</p>
    <p>Footer 2</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on the expected "A string of text".
  auto* text = To<Text>(GetDocument().getElementById("expected")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(16u, markers.at(0)->EndOffset());
}

// Test context terms separated by elements and whitespace
TEST_F(TextFragmentAnchorTest, CrossElementAndWhitespaceContext) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=List%202-,Cat,-Good%20cat",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=List%202-,Cat,-Good%20cat");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <h1> List 1 </h1>
    <div>
      <p>Cat</p>
      <p>&nbsp;Good cat</p>
    </div>
    <h1> List 2 </h1>
    <div>
      <p id="expected">Cat</p>
      <p>&nbsp;Good cat</p>
    </div>
    <h1> List 2 </h1>
    <div>
      <p>Cat</p>
      <p>&nbsp;Bad cat</p>
    </div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on the expected "cat".
  auto* text = To<Text>(GetDocument().getElementById("expected")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(3u, markers.at(0)->EndOffset());
}

// Test context terms separated by empty sibling and parent elements
TEST_F(TextFragmentAnchorTest, CrossEmptySiblingAndParentElementContext) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=prefix-,match,-suffix",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=prefix-,match,-suffix");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>
      <p>prefix</p>
    <div>
    <p><br>&nbsp;</p>
    <div id="expected">match</div>
    <p><br>&nbsp;</p>
    <div>
      <p>suffix</p>
    <div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "match".
  auto* text = To<Text>(GetDocument().getElementById("expected")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(5u, markers.at(0)->EndOffset());
}

// Ensure we scroll to text when its prefix and suffix are out of view.
TEST_F(TextFragmentAnchorTest, DistantElementContext) {
  SimRequest request(
      "https://example.com/test.html#:~:text=Prefix-,Cats,-Suffix",
      "text/html");
  LoadURL("https://example.com/test.html#:~:text=Prefix-,Cats,-Suffix");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 3000px;
      }
    </style>
    <p>Cats</p>
    <p>Prefix</p>
    <p id="text">Cats</p>
    <p>Suffix</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test specifying just one of the prefix and suffix
TEST_F(TextFragmentAnchorTest, OneContextTerm) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test-,page&text=page,-with%20real%20content",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test-,page&text=page,-with%20real%20content");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text1">This is a test page</p>
    <p id="text2">Not a page with real content</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  // Expect marker on the first "page"
  auto* text1 = To<Text>(GetDocument().getElementById("text1")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(15u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on the second "page"
  auto* text2 = To<Text>(GetDocument().getElementById("text2")->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(6u, markers.at(0)->StartOffset());
  EXPECT_EQ(10u, markers.at(0)->EndOffset());
}

// Test that a user scroll cancels the scroll into view.
TEST_F(TextFragmentAnchorTest, ScrollCancelled) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  SimSubresourceRequest css_request("https://example.com/test.css", "text/css");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
        visibility: hidden;
      }
    </style>
    <link rel=stylesheet href=test.css>
    <p id="text">This is a test page</p>
  )HTML");

  Compositor().PaintFrame();
  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 100),
                                                   kUserScroll);

  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor.
  css_request.Complete("p { visibility: visible; top: 1001px; }");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");
  EXPECT_FALSE(ViewportRect().Contains(BoundingRectInFrame(p)));

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "test"
  auto* text = To<Text>(p.firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(14u, markers.at(0)->EndOffset());
}

// Ensure that the text fragment anchor has no effect in an iframe. This is
// disabled in iframes by design, for security reasons.
TEST_F(TextFragmentAnchorTest, DisabledInIframes) {
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html#:~:text=test",
                           "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="iframe" src="child.html#:~:text=test"></iframe>
  )HTML");

  child_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <p>
      test
    </p>
  )HTML");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  Element* iframe = GetDocument().getElementById("iframe");
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());

  EXPECT_EQ(ScrollOffset(),
            child_frame->View()->GetScrollableArea()->GetScrollOffset());
}

// Similarly to the iframe case, we also want to prevent activating a text
// fragment anchor inside a window.opened window.
TEST_F(TextFragmentAnchorTest, DisabledInWindowOpen) {
  String destination = "https://example.com/child.html#:~:text=test";

  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request(destination, "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();

  LocalDOMWindow* main_window = GetDocument().GetFrame()->DomWindow();

  ScriptState* script_state =
      ToScriptStateForMainWorld(main_window->GetFrame());
  ScriptState::Scope entered_context_scope(script_state);
  LocalDOMWindow* child_window = To<LocalDOMWindow>(
      main_window->open(script_state->GetIsolate(), destination, "frame1", "",
                        ASSERT_NO_EXCEPTION));
  ASSERT_TRUE(child_window);

  RunPendingTasks();
  child_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <p>
      test
    </p>
  )HTML");

  RunAsyncMatchingTasks();

  LocalFrameView* child_view = child_window->GetFrame()->View();
  EXPECT_EQ(ScrollOffset(), child_view->GetScrollableArea()->GetScrollOffset());
}

// Ensure that the text fragment anchor is only allowed in full (non-same-page)
// navigations.
TEST_F(TextFragmentAnchorTest, DisabledInSamePageNavigation) {
  SimRequest main_request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <p>
      test
    </p>
  )HTML");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  ASSERT_EQ(ScrollOffset(),
            GetDocument().View()->GetScrollableArea()->GetScrollOffset());

  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope entered_context_scope(script_state);
  GetDocument().GetFrame()->DomWindow()->location()->setHash(
      script_state->GetIsolate(), "text=test", ASSERT_NO_EXCEPTION);
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
}

// Ensure matching is case insensitive.
TEST_F(TextFragmentAnchorTest, CaseInsensitive) {
  SimRequest request("https://example.com/test.html#:~:text=Test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=Test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">test</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test that the fragment anchor stays centered in view throughout loading.
TEST_F(TextFragmentAnchorTest, TargetStaysInView) {
  SimRequest main_request("https://example.com/test.html#:~:text=test",
                          "text/html");
  SimRequest image_request("https://example.com/image.svg", "image/svg+xml");
  LoadURL("https://example.com/test.html#:~:text=test");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <img src="image.svg">
    <p id="text">test</p>
  )HTML");
  Compositor().PaintFrame();
  RunAsyncMatchingTasks();

  ScrollOffset first_scroll_offset = LayoutViewport()->GetScrollOffset();
  ASSERT_NE(ScrollOffset(), first_scroll_offset);

  Element& p = *GetDocument().getElementById("text");
  IntRect first_bounding_rect = BoundingRectInFrame(p);
  EXPECT_TRUE(ViewportRect().Contains(first_bounding_rect));

  // Load an image that pushes the target text out of view
  image_request.Complete(R"SVG(
    <svg xmlns="http://www.w3.org/2000/svg" width="200" height="2000">
      <rect fill="green" width="200" height="2000"/>
    </svg>
  )SVG");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  // Ensure the target text is still in view and stayed centered
  ASSERT_NE(first_scroll_offset, LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
  EXPECT_EQ(first_bounding_rect, BoundingRectInFrame(p));

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test that overlapping text ranges results in only the first one highlighted
TEST_F(TextFragmentAnchorTest, OverlappingTextRanges) {
  SimRequest request(
      "https://example.com/test.html#:~:text=This,test&text=is,page",
      "text/html");
  LoadURL("https://example.com/test.html#:~:text=This,test&text=is,page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "This is a test".
  auto* text = To<Text>(GetDocument().getElementById("text")->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(14u, markers.at(0)->EndOffset());
}

// Test matching a space to &nbsp character.
TEST_F(TextFragmentAnchorTest, SpaceMatchesNbsp) {
  SimRequest request("https://example.com/test.html#:~:text=test%20page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test&nbsp;page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test matching text with a CSS text transform.
TEST_F(TextFragmentAnchorTest, CSSTextTransform) {
  SimRequest request("https://example.com/test.html#:~:text=test%20page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
        text-transform: uppercase;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test that we scroll the element fragment into view if we don't find a match.
TEST_F(TextFragmentAnchorTest, NoMatchFoundFallsBackToElementFragment) {
  SimRequest request("https://example.com/test.html#element:~:text=cats",
                     "text/html");
  LoadURL("https://example.com/test.html#element:~:text=cats");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #text {
        position: absolute;
        top: 1000px;
      }
      #element {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p>This is a test page</p>
    <div id="element">Some text</div>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  // The TextFragmentAnchor needs another frame to invoke the element anchor
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(GetDocument().Url(), "https://example.com/test.html#element");

  Element& p = *GetDocument().getElementById("element");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Test that we don't match partial words at the beginning or end of the text.
TEST_F(TextFragmentAnchorTest, CheckForWordBoundary) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=This%20is%20a%20te&tagetText=st%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=This%20is%20a%20te&tagetText=st%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(GetDocument().Markers().Markers().IsEmpty());
}

// Test that we don't match partial words with context
TEST_F(TextFragmentAnchorTest, CheckForWordBoundaryWithContext) {
  SimRequest request("https://example.com/test.html#:~:text=est-,page",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=est-,page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(GetDocument().Markers().Markers().IsEmpty());
}

// Test that we correctly match a whole word when it appears as a partial word
// earlier in the page.
TEST_F(TextFragmentAnchorTest, CheckForWordBoundaryWithPartialWord) {
  SimRequest request("https://example.com/test.html#:~:text=tes,age",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=tes,age");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">This is a tes age</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  Element& p = *GetDocument().getElementById("second");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "Should have scrolled <p> into view but didn't, scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  // Expect marker on only "tes age"
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *To<Text>(p.firstChild()), DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(17u, markers.at(0)->EndOffset());
}

// Test dismissing the text highlight with a click
TEST_F(TextFragmentAnchorTest, DismissTextHighlightWithClick) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  SimulateClick(100, 100);

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// Test dismissing the text highlight with a tap
TEST_F(TextFragmentAnchorTest, DismissTextHighlightWithTap) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #first {
        position: absolute;
        top: 1000px;
      }
      #second {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="first">This is a test page</p>
    <p id="second">With some more text</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  SimulateTap(100, 100);

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// Test that we don't dismiss a text highlight before it's scrolled into view
TEST_F(TextFragmentAnchorTest, DismissTextHighlightOutOfView) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  SimSubresourceRequest css_request("https://example.com/test.css", "text/css");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
        visibility: hidden;
      }
    </style>
    <link rel=stylesheet href=test.css>
    <p id="text">This is a test page</p>
  )HTML");

  Compositor().PaintFrame();
  ASSERT_EQ(0u, GetDocument().Markers().Markers().size());
  SimulateClick(100, 100);

  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor.
  css_request.Complete("p { visibility: visible; top: 1001px; }");

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Click to dismiss
  SimulateClick(100, 100);
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// Test dismissing a text highlight that didn't require a scroll into view
TEST_F(TextFragmentAnchorTest, DismissTextHighlightInView) {
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=test%20page&text=more%20text");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 100px;
      }
    </style>
    <p>This is a test page</p>
  )HTML");
  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  SimulateTap(100, 100);

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Ensure the fragment is uninstalled
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
}

// Test that the fragment directive delimiter :~: works properly and is stripped
// from the URL.
TEST_F(TextFragmentAnchorTest, FragmentDirectiveDelimiter) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(GetDocument().Url(), "https://example.com/test.html");
}

// Test that a :~: fragment directive is scrolled into view and is stripped from
// the URL when there's also a valid element fragment.
TEST_F(TextFragmentAnchorTest, FragmentDirectiveDelimiterWithElementFragment) {
  SimRequest request("https://example.com/test.html#element:~:text=test",
                     "text/html");
  LoadURL("https://example.com/test.html#element:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      #text {
        position: absolute;
        top: 1000px;
      }
      #element {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="text">This is a test page</p>
    <div id="element">Some text</div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  EXPECT_EQ(GetDocument().Url(), "https://example.com/test.html#element");

  Element& p = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Test that a fragment directive is stripped from the URL even if it is not a
// text directive.
TEST_F(TextFragmentAnchorTest, IdFragmentWithFragmentDirective) {
  SimRequest request("https://example.com/test.html#element:~:id", "text/html");
  LoadURL("https://example.com/test.html#element:~:id");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 2200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
      div {
        position: absolute;
        top: 2000px;
      }
    </style>
    <p id="element">This is a test page</p>
    <div id="element:~:id">Some text</div>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& div = *GetDocument().getElementById("element");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(div)))
      << "Should have scrolled <div> into view but didn't, scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Ensure we can match <text> inside of a <svg> element.
TEST_F(TextFragmentAnchorTest, TextDirectiveInSvg) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      svg {
        position: absolute;
        top: 1000px;
      }
    </style>
    <svg><text id="text" x="0" y="15">This is a test page</text></svg>
  )HTML");
  Compositor().BeginFrame();

  RunAsyncMatchingTasks();

  Element& text = *GetDocument().getElementById("text");

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(text)))
      << "<text> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure we restore the text highlight on page reload
TEST_F(TextFragmentAnchorTest, HighlightOnReload) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  LoadURL("https://example.com/test.html#:~:text=test");
  const String& html = R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      p {
        position: absolute;
        top: 1000px;
      }
    </style>
    <p id="text">This is a test page</p>
  )HTML";
  request.Complete(html);

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Tap to dismiss the highlight.
  SimulateClick(10, 10);
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Reload the page and expect the highlight to be restored.
  SimRequest reload_request("https://example.com/test.html#:~:text=test",
                            "text/html");
  MainFrame().StartReload(WebFrameLoadType::kReload);
  reload_request.Complete(html);

  Compositor().BeginFrame();
  RunAsyncMatchingTasks();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

}  // namespace

}  // namespace blink
