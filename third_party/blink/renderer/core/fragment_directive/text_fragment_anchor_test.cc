// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_test_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

namespace blink {

namespace {

using test::RunPendingTasks;

class TextFragmentAnchorTestController : public TextFragmentAnchorTestBase {
 public:
  explicit TextFragmentAnchorTestController(
      base::test::TaskEnvironment::TimeSource time_source)
      : TextFragmentAnchorTestBase(time_source) {}
  TextFragmentAnchorTestController() = default;

  void BeginEmptyFrame() {
    // If a test case doesn't find a match and therefore doesn't schedule the
    // beforematch event, we should still render a second frame as if we did
    // schedule the event to retain test coverage.
    // When the beforematch event is not scheduled, a DCHECK will fail on
    // BeginFrame() because no event was scheduled, so we schedule an empty task
    // here.
    GetDocument().EnqueueAnimationFrameTask(WTF::BindOnce([]() {}));
    Compositor().BeginFrame();
  }

  ScrollableArea* LayoutViewport() {
    return GetDocument().View()->LayoutViewport();
  }

  gfx::Rect ViewportRect() {
    return gfx::Rect(LayoutViewport()->VisibleContentRect().size());
  }

  gfx::Rect BoundingRectInFrame(Node& node) {
    return node.GetLayoutObject()->AbsoluteBoundingBoxRect();
  }

  void SimulateClick(int x, int y) {
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                        gfx::PointF(x, y), WebPointerProperties::Button::kLeft,
                        0, WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    WebView().MainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
        WebCoalescedInputEvent(event, ui::LatencyInfo()));
  }

  void SimulateRightClick(int x, int y) {
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                        gfx::PointF(x, y), WebPointerProperties::Button::kRight,
                        0, WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    WebView().MainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
        WebCoalescedInputEvent(event, ui::LatencyInfo()));
  }

  void SimulateTap(int x, int y) {
    InjectEvent(WebInputEvent::Type::kTouchStart, x, y);
    InjectEvent(WebInputEvent::Type::kTouchEnd, x, y);
    InjectEvent(WebInputEvent::Type::kGestureTapDown, x, y);
    InjectEvent(WebInputEvent::Type::kGestureTapUnconfirmed, x, y);
    InjectEvent(WebInputEvent::Type::kGestureShowPress, x, y);
    InjectEvent(WebInputEvent::Type::kGestureTap, x, y);
  }

  void LoadAhem() {
    std::optional<Vector<char>> data =
        test::ReadFromFile(test::CoreTestDataPath("Ahem.ttf"));
    ASSERT_TRUE(data);
    auto* buffer =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            DOMArrayBuffer::Create(base::as_byte_span(*data)));
    FontFace* ahem = FontFace::Create(GetDocument().GetFrame()->DomWindow(),
                                      AtomicString("Ahem"), buffer,
                                      FontFaceDescriptors::Create());

    ScriptState* script_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    DummyExceptionStateForTesting exception_state;
    FontFaceSetDocument::From(GetDocument())
        ->addForBinding(script_state, ahem, exception_state);
  }

 private:
  void InjectEvent(WebInputEvent::Type type, int x, int y) {
    if (WebInputEvent::IsGestureEventType(type)) {
      WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                            base::TimeTicks::Now(),
                            WebGestureDevice::kTouchscreen);
      event.SetPositionInWidget(gfx::PointF(x, y));
      event.SetPositionInScreen(gfx::PointF(x, y));
      event.SetFrameScale(1);

      WebView().MainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(event, ui::LatencyInfo()));
    } else if (WebInputEvent::IsTouchEventType(type)) {
      WebTouchEvent event(type, WebInputEvent::kNoModifiers,
                          base::TimeTicks::Now());
      event.SetFrameScale(1);

      WebPointerProperties pointer(0, WebPointerProperties::PointerType::kTouch,
                                   WebPointerProperties::Button::kNoButton,
                                   gfx::PointF(x, y), gfx::PointF(x, y));
      event.touches[0] = pointer;
      if (type == WebInputEvent::Type::kTouchStart)
        event.touches[0].state = WebTouchPoint::State::kStatePressed;
      else if (type == WebInputEvent::Type::kTouchEnd)
        event.touches[0].state = WebTouchPoint::State::kStateReleased;

      WebView().MainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(event, ui::LatencyInfo()));
      WebView().MainFrameWidget()->DispatchBufferedTouchEvents();
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Only needed to support Gesture/Touch until now. "
             "Implement others if new modality is needed.";
    }
  }
};

class TextFragmentAnchorTest : public TextFragmentAnchorTestController {
 public:
  TextFragmentAnchorTest()
      : TextFragmentAnchorTestController(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& p = *GetDocument().getElementById(AtomicString("text"));

  EXPECT_EQ(p, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
      << "<p> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Make sure an anchor isn't created (and we don't crash) if text= is empty.
TEST_F(TextFragmentAnchorTest, EmptyText) {
  SimRequest request("https://example.com/test.html#:~:text=", "text/html");
  LoadURL("https://example.com/test.html#:~:text=");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id="text">This is a test page</p>
  )HTML");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
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
  RunUntilTextFragmentFinalization();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());

  // Force a layout
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("height: 1300px"));
  Compositor().BeginFrame();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
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
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById(AtomicString("first"));

  EXPECT_EQ(first, *GetDocument().CssTarget());
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
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& match = *GetDocument().getElementById(AtomicString("match"));

  EXPECT_EQ(match, *GetDocument().CssTarget());
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
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& first = *GetDocument().getElementById(AtomicString("first"));

  EXPECT_EQ(first, *GetDocument().CssTarget());
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
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& second = *GetDocument().getElementById(AtomicString("second"));

  EXPECT_EQ(second, *GetDocument().CssTarget());
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
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& p = *GetDocument().getElementById(AtomicString("text"));

  EXPECT_EQ(p, *GetDocument().CssTarget());
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
  RunUntilTextFragmentFinalization();
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());

  // Force a layout
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("height: 1300px"));
  Compositor().BeginFrame();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("text")),
            *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "This is a test page".
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("text"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().body(), *GetDocument().CssTarget());
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  auto* text1 = To<Text>(
      GetDocument().getElementById(AtomicString("text1"))->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on "with another paragraph"
  auto* text2 = To<Text>(
      GetDocument().getElementById(AtomicString("text2"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().body(), *GetDocument().CssTarget());
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  auto* text1 = To<Text>(
      GetDocument().getElementById(AtomicString("text1"))->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on "with another paragraph"
  auto* text2 = To<Text>(
      GetDocument().getElementById(AtomicString("text2"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().body(), *GetDocument().CssTarget());
  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  // Expect marker on "test page"
  auto* text1 = To<Text>(
      GetDocument().getElementById(AtomicString("text1"))->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect markers on "with" and "paragraph of text"
  auto* text2 = To<Text>(
      GetDocument().getElementById(AtomicString("text2"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("text")),
            *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "is a test".
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("text"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("expected")),
            *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on the expected "A string of text".
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("expected"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("expected")),
            *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on the expected "cat".
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("expected"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("expected")),
            *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "match".
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("expected"))->firstChild());
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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("text1")),
            *GetDocument().CssTarget());

  // Expect marker on the first "page"
  auto* text1 = To<Text>(
      GetDocument().getElementById(AtomicString("text1"))->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text1, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(15u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());

  // Expect marker on the second "page"
  auto* text2 = To<Text>(
      GetDocument().getElementById(AtomicString("text2"))->firstChild());
  markers = GetDocument().Markers().MarkersFor(
      *text2, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(6u, markers.at(0)->StartOffset());
  EXPECT_EQ(10u, markers.at(0)->EndOffset());
}

class TextFragmentAnchorScrollTest
    : public TextFragmentAnchorTest,
      public testing::WithParamInterface<mojom::blink::ScrollType> {
 protected:
  bool IsUserScrollType() {
    return GetParam() == mojom::blink::ScrollType::kCompositor ||
           GetParam() == mojom::blink::ScrollType::kUser;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ScrollTypes,
    TextFragmentAnchorScrollTest,
    testing::Values(mojom::blink::ScrollType::kUser,
                    mojom::blink::ScrollType::kProgrammatic,
                    mojom::blink::ScrollType::kClamping,
                    mojom::blink::ScrollType::kCompositor,
                    mojom::blink::ScrollType::kAnchoring,
                    mojom::blink::ScrollType::kSequenced));

// Test that a user scroll cancels the scroll into view.
TEST_P(TextFragmentAnchorScrollTest, ScrollCancelled) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  SimSubresourceRequest css_request("https://example.com/test.css", "text/css");
  SimSubresourceRequest img_request("https://example.com/test.png",
                                    "image/png");
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
    <img src="test.png">
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  mojom::blink::ScrollType scroll_type = GetParam();

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 100),
                                                   scroll_type);
  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor in the next begin frame.
  css_request.Complete("p { visibility: visible; top: 1001px; }");
  img_request.Complete("");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  Element& p = *GetDocument().getElementById(AtomicString("text"));

  // If the scroll was a user scroll then we shouldn't try to keep the fragment
  // in view. Otherwise, we should.
  if (IsUserScrollType()) {
    EXPECT_FALSE(ViewportRect().Contains(BoundingRectInFrame(p)));
  } else {
    EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
  }

  EXPECT_EQ(p, *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "test"
  auto* text = To<Text>(p.firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(10u, markers.at(0)->StartOffset());
  EXPECT_EQ(14u, markers.at(0)->EndOffset());
}

// Test that user scrolling doesn't dismiss the highlight.
TEST_P(TextFragmentAnchorScrollTest, DontDismissTextHighlightOnUserScroll) {
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
  RunUntilTextFragmentFinalization();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  ASSERT_EQ(2u, GetDocument().Markers().Markers().size());

  mojom::blink::ScrollType scroll_type = GetParam();
  LayoutViewport()->ScrollBy(ScrollOffset(0, -10), scroll_type);

  Compositor().BeginFrame();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
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
  RunPendingTasks();
  Compositor().BeginFrame();

  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());

  EXPECT_FALSE(child_frame->View()->GetFragmentAnchor());
  EXPECT_EQ(nullptr, GetDocument().CssTarget());
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
      main_window->open(script_state->GetIsolate(), destination,
                        AtomicString("frame1"), "", ASSERT_NO_EXCEPTION));
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

  EXPECT_EQ(nullptr, child_window->document()->CssTarget());

  LocalFrameView* child_view = child_window->GetFrame()->View();
  EXPECT_EQ(ScrollOffset(), child_view->GetScrollableArea()->GetScrollOffset());
}

// Ensure that the text fragment anchor is not activated by same-document script
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
  RunPendingTasks();
  Compositor().BeginFrame();

  ASSERT_EQ(ScrollOffset(),
            GetDocument().View()->GetScrollableArea()->GetScrollOffset());

  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope entered_context_scope(script_state);
  GetDocument().GetFrame()->DomWindow()->location()->setHash(
      script_state->GetIsolate(), ":~:text=test", ASSERT_NO_EXCEPTION);
  RunAsyncMatchingTasks();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));

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
  RunAsyncMatchingTasks();
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().IsLoadCompleted());
  EXPECT_TRUE(GetDocument().HasFinishedParsing());

  ScrollOffset first_scroll_offset = LayoutViewport()->GetScrollOffset();
  ASSERT_NE(ScrollOffset(), first_scroll_offset);

  Element& p = *GetDocument().getElementById(AtomicString("text"));
  gfx::Rect first_bounding_rect = BoundingRectInFrame(p);
  EXPECT_TRUE(ViewportRect().Contains(first_bounding_rect));

  // Load an image that pushes the target text out of view
  image_request.Complete(R"SVG(
    <svg xmlns="http://www.w3.org/2000/svg" width="200" height="2000">
      <rect fill="green" width="200" height="2000"/>
    </svg>
  )SVG");
  RunPendingTasks();
  EXPECT_TRUE(GetDocument().IsLoadCompleted());
  EXPECT_TRUE(GetDocument().HasFinishedParsing());

  Compositor().BeginFrame();

  // Ensure the target text is still in view and stayed centered
  ASSERT_NE(first_scroll_offset, LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
  EXPECT_EQ(first_bounding_rect, BoundingRectInFrame(p));

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test that overlapping text ranges results in both highlights with
// a merged highlight.
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Expect marker on "This is a test page".
  auto* text = To<Text>(
      GetDocument().getElementById(AtomicString("text"))->firstChild());
  DocumentMarkerVector markers = GetDocument().Markers().MarkersFor(
      *text, DocumentMarker::MarkerTypes::TextFragment());
  ASSERT_EQ(1u, markers.size());
  EXPECT_EQ(0u, markers.at(0)->StartOffset());
  EXPECT_EQ(19u, markers.at(0)->EndOffset());
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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));

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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));

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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("element"));

  // At this point, the anchor should have been cleaned up.
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  // The text directive should be removed from the URL.
  EXPECT_EQ(GetDocument().Url(), "https://example.com/test.html#element");

  // Ensure the element was scrolled into view.
  ASSERT_TRUE(GetDocument().CssTarget());
  EXPECT_EQ(p, *GetDocument().CssTarget());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(nullptr, GetDocument().CssTarget());
  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
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
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("second"));

  EXPECT_EQ(p, *GetDocument().CssTarget());
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

// Test click keeps the text highlight
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
  RunUntilTextFragmentFinalization();
  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());

  KURL url = GetDocument()
                 .GetFrame()
                 ->Loader()
                 .GetDocumentLoader()
                 ->GetHistoryItem()
                 ->Url();
  EXPECT_EQ(
      "https://example.com/test.html#:~:text=test%20page&text=more%20text",
      url.GetString());
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  SimulateClick(100, 100);

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  url = GetDocument()
            .GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetHistoryItem()
            ->Url();
  EXPECT_EQ(
      "https://example.com/test.html#:~:text=test%20page&text=more%20text",
      url.GetString());
}

// Test not dismissing the text highlight with a click.
TEST_F(TextFragmentAnchorTest, DontDismissTextHighlightWithClick) {
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  SimulateClick(100, 100);

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Test that a tap keeps the text highlight
TEST_F(TextFragmentAnchorTest, KeepsTextHighlightWithTap) {
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
  RunUntilTextFragmentFinalization();

  KURL url = GetDocument()
                 .GetFrame()
                 ->Loader()
                 .GetDocumentLoader()
                 ->GetHistoryItem()
                 ->Url();
  EXPECT_EQ(
      "https://example.com/test.html#:~:text=test%20page&text=more%20text",
      url.GetString());
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  SimulateTap(100, 100);

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  url = GetDocument()
            .GetFrame()
            ->Loader()
            .GetDocumentLoader()
            ->GetHistoryItem()
            ->Url();
  EXPECT_EQ(
      "https://example.com/test.html#:~:text=test%20page&text=more%20text",
      url.GetString());
}

// Test not dismissing the text highlight with a tap.
TEST_F(TextFragmentAnchorTest, DontDismissTextHighlightWithTap) {
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  SimulateTap(100, 100);

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Test that we don't dismiss a text highlight before and after it's scrolled
// into view
TEST_F(TextFragmentAnchorTest, KeepsTextHighlightOutOfView) {
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

  ASSERT_EQ(0u, GetDocument().Markers().Markers().size());
  SimulateClick(100, 100);

  // Set the target text to visible and change its position to cause a layout
  // and invoke the fragment anchor.
  css_request.Complete("p { visibility: visible; top: 1001px; }");
  RunAsyncMatchingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Click
  SimulateClick(100, 100);
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Test that a text highlight that didn't require a scroll into view is kept on
// tap
TEST_F(TextFragmentAnchorTest, KeepsTextHighlightInView) {
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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  SimulateTap(100, 100);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
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
  RunUntilTextFragmentFinalization();

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
  RunUntilTextFragmentFinalization();

  EXPECT_EQ(GetDocument().Url(), "https://example.com/test.html#element");

  Element& p = *GetDocument().getElementById(AtomicString("text"));

  EXPECT_EQ(p, *GetDocument().CssTarget());
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
  RunPendingTasks();
  Compositor().BeginFrame();

  Element& p = *GetDocument().getElementById(AtomicString("element"));

  EXPECT_EQ(p, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)))
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
  RunUntilTextFragmentFinalization();

  Element& text = *GetDocument().getElementById(AtomicString("text"));

  EXPECT_EQ(text, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(text)))
      << "<text> Element wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure we restore the text highlight on page reload
// TODO(bokan): This test is disabled as this functionality was suppressed in
// https://crrev.com/c/2135407; it would be better addressed by providing a
// highlight-only function. See the TODO in
// https://wicg.github.io/ScrollToTextFragment/#restricting-the-text-fragment
TEST_F(TextFragmentAnchorTest, DISABLED_HighlightOnReload) {
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
  RunUntilTextFragmentFinalization();

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

  EXPECT_EQ(*GetDocument().getElementById(AtomicString("text")),
            *GetDocument().CssTarget());
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure that we can have text directives combined with non-text directives
TEST_F(TextFragmentAnchorTest, NonTextDirectives) {
  SimRequest request(
      "https://example.com/test.html#:~:text=test&directive&text=more",
      "text/html");
  LoadURL("https://example.com/test.html#:~:text=test&directive&text=more");
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
  RunUntilTextFragmentFinalization();

  Element& first = *GetDocument().getElementById(AtomicString("first"));

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(first)))
      << "First <p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Test that the text directive applies :target styling
TEST_F(TextFragmentAnchorTest, CssTarget) {
  SimRequest main_request("https://example.com/test.html#:~:text=test",
                          "text/html");
  SimRequest css_request("https://example.com/test.css", "text/css");
  LoadURL("https://example.com/test.html#:~:text=test");
  main_request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      p {
        margin-top: 1000px;
      }
    </style>
    <link rel="stylesheet" href="test.css">
    <p id="text">test</p>
  )HTML");

  css_request.Complete(R"CSS(
    :target {
      margin-top: 2000px;
    }
  )CSS");
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Ensure the text fragment anchor matching only occurs after the page becomes
// visible.
TEST_F(TextFragmentAnchorTest, PageVisibility) {
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*initial_state=*/true);
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
  test::RunPendingTasks();

  // Render two frames and ensure matching and scrolling does not occur.
  BeginEmptyFrame();
  BeginEmptyFrame();

  Element& p = *GetDocument().getElementById(AtomicString("text"));
  EXPECT_FALSE(ViewportRect().Contains(BoundingRectInFrame(p)));
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(nullptr, GetDocument().CssTarget());

  // Set the page visible and verify the match.
  WebView().SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*initial_state=*/false);
  BeginEmptyFrame();
  BeginEmptyFrame();

  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(p, *GetDocument().CssTarget());
}

// Regression test for https://crbug.com/1147568. Make sure a page setting
// manual scroll restoration doesn't cause the fragment to avoid scrolling on
// the initial load.
TEST_F(TextFragmentAnchorTest, ManualRestorationDoesntBlockFragment) {
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
    <script>
      history.scrollRestoration = 'manual';
    </script>
    <p id="text">This is a test page</p>
  )HTML");
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
}

// Regression test for https://crbug.com/1147453. Ensure replaceState doesn't
// clobber the text fragment token and allows fragment to scroll.
TEST_F(TextFragmentAnchorTest, ReplaceStateDoesntBlockFragment) {
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
    <script>
      history.replaceState({}, 'test', '');
    </script>
    <p id="text">This is a test page</p>
  )HTML");
  RunUntilTextFragmentFinalization();

  Element& p = *GetDocument().getElementById(AtomicString("text"));
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(p)));
}

// Test that a text directive can match across comment nodes
TEST_F(TextFragmentAnchorTest, MatchAcrossCommentNode) {
  SimRequest request("https://example.com/test.html#:~:text=abcdef",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=abcdef");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      div {
        position: absolute;
        top: 1000px;
      }
    </style>
    <div id="text"><span>abc</span><!--comment--><span>def</span></div>
  )HTML");
  RunUntilTextFragmentFinalization();

  Element& div = *GetDocument().getElementById(AtomicString("text"));

  EXPECT_EQ(div, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(div)));
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
}

// Test that selection is successful for same prefix and text start.
TEST_F(TextFragmentAnchorTest, SamePrefixAndText) {
  SimRequest request("https://example.com/test.html#:~:text=foo-,foo,-bar",
                     "text/html");
  LoadURL("https://example.com/test.html#:~:text=foo-,foo,-bar");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      div {
        position: absolute;
        top: 1000px;
      }
    </style>
    <div id="text">foo foo foo bar bar bar</div>
  )HTML");
  RunUntilTextFragmentFinalization();

  Element& div = *GetDocument().getElementById(AtomicString("text"));

  EXPECT_EQ(div, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(div)));
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

// Checks that selection in the same text node is considerered uninterrupted.
TEST_F(TextFragmentAnchorTest, IsInSameUninterruptedBlock_OneTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First paragraph text</div>
  )HTML");
  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = PositionInFlatTree(first_paragraph, 0);
  const auto& end = PositionInFlatTree(first_paragraph, 15);
  ASSERT_EQ("First paragraph", PlainText(EphemeralRangeInFlatTree(start, end)));

  EXPECT_TRUE(TextFragmentFinder::IsInSameUninterruptedBlock(start, end));
}

// Checks that selection in the same text node with nested non-block element is
// considerered uninterrupted.
TEST_F(TextFragmentAnchorTest,
       IsInSameUninterruptedBlock_NonBlockInterruption) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <i>styled text</i> paragraph text</div>
  )HTML");
  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = PositionInFlatTree(first_paragraph, 0);
  const auto& end =
      PositionInFlatTree(first_paragraph->nextSibling()->nextSibling(), 10);
  ASSERT_EQ("First styled text paragraph",
            PlainText(EphemeralRangeInFlatTree(start, end)));

  EXPECT_TRUE(TextFragmentFinder::IsInSameUninterruptedBlock(start, end));
}

// Checks that selection in the same text node with nested block element is
// considerered interrupted.
TEST_F(TextFragmentAnchorTest, IsInSameUninterruptedBlock_BlockInterruption) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div>block text</div> paragraph text</div>
  )HTML");
  Node* first_paragraph =
      GetDocument().getElementById(AtomicString("first"))->firstChild();
  const auto& start = PositionInFlatTree(first_paragraph, 0);
  const auto& end =
      PositionInFlatTree(first_paragraph->nextSibling()->nextSibling(), 10);
  ASSERT_EQ("First\nblock text\nparagraph",
            PlainText(EphemeralRangeInFlatTree(start, end)));

  EXPECT_FALSE(TextFragmentFinder::IsInSameUninterruptedBlock(start, end));
}

TEST_F(TextFragmentAnchorTest, OpenedFromHighlightDoesNotSelectAdditionalText) {
  SimRequest request("https://www.test.com/#:~:text=First%20test,page%20three",
                     "text/html");
  LoadURL("https://www.test.com/#:~:text=First%20test,page%20three");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
      p {
        font-size: 12px;
      }
      </style>
      <p id="one">First test page one</p>
      <p id="two">Second test page two</p>
      <p id="three">Third test page three</p>
      <p id="four">Fourth test page four</p>
      </html>)HTML");
  RunUntilTextFragmentFinalization();

  Element* middle_element = GetDocument().getElementById(AtomicString("two"));
  Element* last_element = GetDocument().getElementById(AtomicString("four"));

  WebView().GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingMacBehavior);

  // Create a mouse event in the middle of <p> two.
  WebMouseEvent mouse_down_event(WebInputEvent::Type::kMouseDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
  const DOMRect* middle_rect = middle_element->GetBoundingClientRect();
  gfx::PointF middle_elem_point(((middle_rect->left() + 1)),
                                ((middle_rect->top() + 1)));
  mouse_down_event.SetPositionInWidget(middle_elem_point.x(),
                                       middle_elem_point.y());
  mouse_down_event.SetPositionInScreen(middle_elem_point.x(),
                                       middle_elem_point.y());
  mouse_down_event.click_count = 1;
  mouse_down_event.button = WebMouseEvent::Button::kRight;

  // Corresponding release event (Windows shows context menu on release).
  WebMouseEvent mouse_up_event(mouse_down_event);
  mouse_up_event.SetType(WebInputEvent::Type::kMouseUp);

  WebView().MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event, ui::LatencyInfo()));
  WebView().MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_up_event, ui::LatencyInfo()));

  // No additional text should be selected.
  FrameSelection& selection = GetDocument().GetFrame()->Selection();
  EXPECT_TRUE(selection.SelectedText().empty());

  // Create a mouse event at the center of <p> four.
  const DOMRect* last_rect = last_element->GetBoundingClientRect();
  gfx::PointF last_elem_point(((last_rect->left() + 1)),
                              ((last_rect->top() + 1)));
  mouse_down_event.SetPositionInWidget(last_elem_point.x(),
                                       last_elem_point.y());
  mouse_down_event.SetPositionInScreen(last_elem_point.x(),
                                       last_elem_point.y());

  // Corresponding release event (Windows shows context menu on release).
  WebMouseEvent last_mouse_up_event(mouse_down_event);
  last_mouse_up_event.SetType(WebInputEvent::Type::kMouseUp);

  WebView().MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event, ui::LatencyInfo()));
  WebView().MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(last_mouse_up_event, ui::LatencyInfo()));

  // The text underneath the cursor should be selected.
  EXPECT_FALSE(selection.SelectedText().empty());
}

// Test that on Android, a user can display a context menu by tapping on
// a text fragment, when the TextFragmentTapOpensContextMenu
// RuntimeEnabledFeature is enabled.
TEST_F(TextFragmentAnchorTest, ShouldOpenContextMenuOnTap) {
  LoadAhem();
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=this%20is%20a%20test%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=this%20is%20a%20test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>p { font: 10px/1 Ahem; }</style>
    <p id="first">This is a test page</p>
    <p id="two">Second test page two</p>
  )HTML");
  RunUntilTextFragmentFinalization();

  ContextMenuAllowedScope context_menu_allowed_scope;

  EXPECT_FALSE(GetDocument()
                   .GetPage()
                   ->GetContextMenuController()
                   .ContextMenuNodeForFrame(GetDocument().GetFrame()));

  Range* range = Range::Create(GetDocument());
  range->setStart(GetDocument().getElementById(AtomicString("first")), 0,
                  IGNORE_EXCEPTION_FOR_TESTING);
  range->setEnd(GetDocument().getElementById(AtomicString("first")), 1,
                IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_EQ("This is a test page", range->GetText());

  gfx::Point tap_point = range->BoundingBox().CenterPoint();
  SimulateTap(tap_point.x(), tap_point.y());

  if (RuntimeEnabledFeatures::TextFragmentTapOpensContextMenuEnabled()) {
    EXPECT_TRUE(GetDocument()
                    .GetPage()
                    ->GetContextMenuController()
                    .ContextMenuNodeForFrame(GetDocument().GetFrame()));
  } else {
    EXPECT_FALSE(GetDocument()
                     .GetPage()
                     ->GetContextMenuController()
                     .ContextMenuNodeForFrame(GetDocument().GetFrame()));
  }

  GetDocument().GetPage()->GetContextMenuController().ClearContextMenu();

  range->setStart(GetDocument().getElementById(AtomicString("two")), 0,
                  IGNORE_EXCEPTION_FOR_TESTING);
  range->setEndAfter(GetDocument().getElementById(AtomicString("two")),
                     IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_EQ("Second test page two", range->GetText());

  tap_point = range->BoundingBox().CenterPoint();
  SimulateTap(tap_point.x(), tap_point.y());

  EXPECT_FALSE(GetDocument()
                   .GetPage()
                   ->GetContextMenuController()
                   .ContextMenuNodeForFrame(GetDocument().GetFrame()));
}

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
// Mock implementation of the UnhandledTapNotifier Mojo receiver, for testing
// the ShowUnhandledTapUIIfNeeded notification.
class MockUnhandledTapNotifierImpl : public mojom::blink::UnhandledTapNotifier {
 public:
  MockUnhandledTapNotifierImpl() = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::UnhandledTapNotifier>(
        std::move(handle)));
  }

  void ShowUnhandledTapUIIfNeeded(
      mojom::blink::UnhandledTapInfoPtr unhandled_tap_info) override {
    was_unhandled_tap_ = true;
  }
  bool WasUnhandledTap() const { return was_unhandled_tap_; }
  bool ReceiverIsBound() const { return receiver_.is_bound(); }
  void Reset() {
    was_unhandled_tap_ = false;
    receiver_.reset();
  }

 private:
  bool was_unhandled_tap_ = false;

  mojo::Receiver<mojom::blink::UnhandledTapNotifier> receiver_{this};
};
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
// Test that on Android, when a user taps on a text, ShouldNotRequestUnhandled
// does not get triggered. When a user taps on a highlight, no text should be
// selected. RuntimeEnabledFeature is enabled.
TEST_F(TextFragmentAnchorTest,
       ShouldNotRequestUnhandledTapNotifierWhenTapOnTextFragment) {
  LoadAhem();
  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=this%20is%20a%20test%20page",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=this%20is%20a%20test%20page");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>p { font: 10px/1 Ahem; }</style>
    <p id="first">This is a test page</p>
    <p id="two">Second test page two</p>
  )HTML");
  RunUntilTextFragmentFinalization();

  MockUnhandledTapNotifierImpl mock_notifier;
  GetDocument().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::UnhandledTapNotifier::Name_,
      WTF::BindRepeating(&MockUnhandledTapNotifierImpl::Bind,
                         WTF::Unretained(&mock_notifier)));

  Range* range = Range::Create(GetDocument());
  range->setStart(GetDocument().getElementById(AtomicString("first")), 0,
                  IGNORE_EXCEPTION_FOR_TESTING);
  range->setEnd(GetDocument().getElementById(AtomicString("first")), 1,
                IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_EQ("This is a test page", range->GetText());

  mock_notifier.Reset();
  gfx::Point tap_point = range->BoundingBox().CenterPoint();
  SimulateTap(tap_point.x(), tap_point.y());

  base::RunLoop().RunUntilIdle();
  if (RuntimeEnabledFeatures::TextFragmentTapOpensContextMenuEnabled()) {
    EXPECT_FALSE(mock_notifier.WasUnhandledTap());
    EXPECT_FALSE(mock_notifier.ReceiverIsBound());
  } else {
    EXPECT_TRUE(mock_notifier.WasUnhandledTap());
    EXPECT_TRUE(mock_notifier.ReceiverIsBound());
  }

  range->setStart(GetDocument().getElementById(AtomicString("two")), 0,
                  IGNORE_EXCEPTION_FOR_TESTING);
  range->setEndAfter(GetDocument().getElementById(AtomicString("two")),
                     IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_EQ("Second test page two", range->GetText());

  mock_notifier.Reset();
  tap_point = range->BoundingBox().CenterPoint();
  SimulateTap(tap_point.x(), tap_point.y());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_notifier.WasUnhandledTap());
  EXPECT_TRUE(mock_notifier.ReceiverIsBound());
}
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

TEST_F(TextFragmentAnchorTest, TapOpeningContextMenuWithDirtyLifecycleNoCrash) {
  ScopedTextFragmentTapOpensContextMenuForTest tap_opens_context_menu(true);

  SimRequest request(
      "https://example.com/"
      "test.html#:~:text=This%20is%20just%20example",
      "text/html");
  LoadURL(
      "https://example.com/"
      "test.html#:~:text=This%20is%20just%20example");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <html>
    <head>
      <style>
        .content {
          width: 1000px;
          height: 2000px;
          background-color: silver;
        }
      </style>
      <script>
        // Dirty lifecycle inside the click event.
        addEventListener('click', () => {
          document.body.style.width = '500px';
        });
        // This prevents calling HandleMouseReleaseEvent which has an
        // UpdateLifecycle call inside it but it also prevents showing the
        // context menu.
        addEventListener('mouseup', (e) => { e.preventDefault(); });
      </script>
    </head>

    <body>
      This is just example text that will wrap.
      <div class="content"></div>
    </body>
    </html>
  )HTML");
  RunUntilTextFragmentFinalization();

  ContextMenuAllowedScope context_menu_allowed_scope;

  EXPECT_FALSE(GetDocument()
                   .GetPage()
                   ->GetContextMenuController()
                   .ContextMenuNodeForFrame(GetDocument().GetFrame()));

  Node* first_paragraph = GetDocument().body()->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("This is just example", PlainText(EphemeralRange(start, end)));

  Range* range = CreateRange(EphemeralRange(start, end));

  gfx::Point tap_point = range->BoundingBox().CenterPoint();
  SimulateTap(tap_point.x(), tap_point.y());

  // Expect that we won't see the context menu because we preventDefaulted the
  // mouseup but this test passes if it doesn't crash.
  EXPECT_FALSE(GetDocument()
                   .GetPage()
                   ->GetContextMenuController()
                   .ContextMenuNodeForFrame(GetDocument().GetFrame()));
}

// Test for https://crbug.com/1453658. Trips a CHECK because an AnnotationAgent
// unexpectedly calls Attach a second time after initially succeeding because
// the matched range becomes collapsed.
TEST_F(TextFragmentAnchorTest, InitialMatchingIsCollapsedCrash) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  SimSubresourceRequest sub_request("https://example.com/null.png",
                                    "image/png");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      div {
        position: absolute;
        top: 1000px;
      }
    </style>
    <div id="text">test</div>
    <img src="null.png">
  )HTML");
  // Parsing completed but load is still waiting on the <img>, this will run
  // matching and match "test".
  Compositor().BeginFrame();

  // Ensure we've attached the annotation for the text fragment.
  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  auto annotations = container->GetAgentsOfType(
      mojom::blink::AnnotationType::kSharedHighlight);
  ASSERT_EQ(annotations.size(), 1ul);
  ASSERT_TRUE((*annotations.begin())->IsAttached());

  // Remove the matched text node; this will collapse the matched range.
  Element& div = *GetDocument().getElementById(AtomicString("text"));
  div.firstChild()->remove();
  ASSERT_FALSE((*annotations.begin())->IsAttached());

  // Complete the <img> request (with an error). This will fire the load event
  // and perform another matching pass. Test passes if this doesn't crash.
  sub_request.Complete("");
  Compositor().BeginFrame();
}

// Test the behavior of removing matched text while waiting to expand a
// hidden=until-found section. We mostly care that this doesn't crash or
// violate any state CHECKs.
TEST_F(TextFragmentAnchorTest, InitialMatchPendingBecomesCollapsed) {
  SimRequest request("https://example.com/test.html#:~:text=test", "text/html");
  SimSubresourceRequest sub_request("https://example.com/null.png",
                                    "image/png");
  LoadURL("https://example.com/test.html#:~:text=test");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        height: 1200px;
      }
      div {
        position: absolute;
        top: 1000px;
      }
    </style>
    <div id="text" hidden="until-found">test</div>
    <img src="null.png">
    <div id="second">test (will match on second pass)</div>
  )HTML");
  // Parsing completed but load is still waiting on the <img>, this will run
  // matching and match "test" but queue a rAF task to show the hidden <div>.
  Compositor().BeginFrame();

  // Ensure we've queued the "DomMutation" rAF task.
  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  auto annotations = container->GetAgentsOfType(
      mojom::blink::AnnotationType::kSharedHighlight);
  ASSERT_EQ(annotations.size(), 1ul);
  ASSERT_TRUE((*annotations.begin())->IsAttachmentPending());

  // Remove the matched text node; this will collapse the matched range.
  Element& div = *GetDocument().getElementById(AtomicString("text"));
  div.firstChild()->remove();

  // Complete the <img> request (with an error). This will fire the load event
  // and the UpdateStyleAndLayout will perform another matching pass but this
  // shouldn't re-search the pending match.
  sub_request.Complete("");
  RunPendingTasks();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // This will run the "DomMutation" rAF task from the first match.
  Compositor().BeginFrame();

  // The directive should not have scrolled or created a marker.
  EXPECT_EQ(ScrollOffset(), LayoutViewport()->GetScrollOffset());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
}

// These tests are specifically testing the post-load timer task so use
// the real clock to faithfully reproduce real-world behavior.
class TextFragmentAnchorPostLoadTest : public TextFragmentAnchorTestController {
 public:
  TextFragmentAnchorPostLoadTest() = default;
  void SetUp() override {
    TextFragmentAnchorTestController::SetUp();
    DisableVirtualTimeIfSet();
  }
};

// Ensure a content added shortly after load is found.
TEST_F(TextFragmentAnchorPostLoadTest, ContentAddedPostLoad) {
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
      <div id="spacer"></div>
      <p id="match">Loading...</p>
    </body>
    <script>
      onload = () => {
        requestAnimationFrame(() => requestAnimationFrame(() => {
          document.getElementById('match').firstChild.data = 'A test page';
        }));
      }
    </script>
  )HTML");
  RunUntilTextFragmentFinalization();

  Element& match = *GetDocument().getElementById(AtomicString("match"));
  ASSERT_TRUE(GetDocument().CssTarget());
  EXPECT_EQ(match, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(match)))
      << "<p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Ensure a content added shortly after load is found.
TEST_F(TextFragmentAnchorPostLoadTest, HiddenAfterFoundPostLoad) {
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
      <div id="spacer"></div>
      <p id="match" hidden>A test page</p>
    </body>
    <script>
      onload = () => {
        requestAnimationFrame(() => requestAnimationFrame(() => {
          document.getElementById('match').setAttribute('hidden', 'until-found');
        }));
      }
    </script>
  )HTML");
  RunUntilTextFragmentFinalization();

  Element& match = *GetDocument().getElementById(AtomicString("match"));
  ASSERT_TRUE(GetDocument().CssTarget());
  EXPECT_EQ(match, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(match)))
      << "<p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

// Ensure that the text fragment is searched within the delay time after load if
// DOM hasn't been mutated.
TEST_F(TextFragmentAnchorPostLoadTest, PostLoadSearchEndsWithoutDomMutation) {
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
      <div id="spacer"></div>
      <p id="match">Loading...</p>
    </body>
  )HTML");
  // Ensure the load event is run.
  test::RunPendingTasks();
  Compositor().BeginFrame();

  ASSERT_TRUE(GetDocument().View()->GetFragmentAnchor());

  test::RunDelayedTasks(TextFragmentAnchor::PostLoadTaskDelay());
  Compositor().BeginFrame();

  // Final frame for finalization.
  Compositor().BeginFrame();

  EXPECT_FALSE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());
}

// Ensure that the post-load text fragment search is pushed back each time DOM
// is mutated.
TEST_F(TextFragmentAnchorPostLoadTest, PostLoadSearchTimesOut) {
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
      <div id="spacer"></div>
      <p id="match">Loading...</p>
    </body>
    <script>
      onload = () => {
        requestAnimationFrame(() => {
          document.getElementById('match').innerText = 'A test page';

          setInterval(() => {
            let p = document.createElement('p');
            document.body.appendChild(p);
          }, 50);
        });
      }
    </script>
  )HTML");
  // Ensure the load event is run.
  test::RunPendingTasks();

  // The fragment anchor is created but the search fails because the text
  // is still hidden.
  ASSERT_TRUE(GetDocument().View()->GetFragmentAnchor());
  ASSERT_TRUE(GetDocument().Markers().Markers().empty());

  // The text should now be available.
  Compositor().BeginFrame();
  Element& match = *GetDocument().getElementById(AtomicString("match"));
  ASSERT_EQ("A test page", match.innerText());

  // Waiting for the delay period shouldn't find the text because the DOM is
  // constantly being appended to. However, the search is still active and the
  // FragmentAnchor alive.
  test::RunDelayedTasks(TextFragmentAnchor::PostLoadTaskDelay());
  Compositor().BeginFrame();
  ASSERT_TRUE(GetDocument().View()->GetFragmentAnchor());
  EXPECT_TRUE(GetDocument().Markers().Markers().empty());

  // If we wait for the full timeout amount it should eventually
  // perform the search.
  test::RunDelayedTasks(TextFragmentAnchor::PostLoadTaskTimeout() -
                        TextFragmentAnchor::PostLoadTaskDelay());
  Compositor().BeginFrame();
  EXPECT_FALSE(GetDocument().Markers().Markers().empty());
  EXPECT_EQ(match, *GetDocument().CssTarget());
  EXPECT_TRUE(ViewportRect().Contains(BoundingRectInFrame(match)))
      << "<p> wasn't scrolled into view, viewport's scroll offset: "
      << LayoutViewport()->GetScrollOffset().ToString();
}

}  // namespace

}  // namespace blink
