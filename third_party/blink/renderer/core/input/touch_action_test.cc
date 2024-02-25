/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using blink::test::RunPendingTasks;

namespace blink {

class TouchActionWidgetInputHandlerHost
    : public frame_test_helpers::TestWidgetInputHandlerHost {
 public:
  void SetTouchActionFromMain(TouchAction touch_action) override {
    action_set_count_++;
    action_ = touch_action;
  }

  void ResetTouchAction() {
    action_set_count_ = 0;
    action_ = TouchAction::kAuto;
  }

  int action_set_count() const { return action_set_count_; }

  TouchAction action() const { return action_; }

 private:
  int action_set_count_ = 0;
  TouchAction action_ = TouchAction::kAuto;
};

class TouchActionTrackingWebFrameWidget
    : public frame_test_helpers::TestWebFrameWidget {
 public:
  template <typename... Args>
  explicit TouchActionTrackingWebFrameWidget(Args&&... args)
      : frame_test_helpers::TestWebFrameWidget(std::forward<Args>(args)...) {}

  // frame_test_helpers::TestWebFrameWidget overrides.
  frame_test_helpers::TestWidgetInputHandlerHost* GetInputHandlerHost()
      override {
    return &input_handler_host_;
  }

  void Reset() { input_handler_host_.ResetTouchAction(); }

  int TouchActionSetCount() { return input_handler_host_.action_set_count(); }

  TouchAction LastTouchAction() { return input_handler_host_.action(); }

 private:
  TouchActionWidgetInputHandlerHost input_handler_host_;
};

class TouchActionTest : public testing::Test {
 public:
  TouchActionTest()
      : base_url_("http://www.test.com/"),
        web_view_helper_(WTF::BindRepeating(
            &frame_test_helpers::WebViewHelper::CreateTestWebFrameWidget<
                TouchActionTrackingWebFrameWidget>)) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(),
        "touch-action-tests.css", "text/css");
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), "touch-action-tests.js",
        "text/javascript");
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), "white-1x1.png",
        "image/png");
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void RunTouchActionTest(String file);
  void RunShadowDOMTest(String file);
  void RunIFrameTest(String file);
  void SendTouchEvent(WebView*, WebInputEvent::Type, gfx::Point client_point);
  WebViewImpl* SetupTest(String file);
  void RunTestOnTree(ContainerNode* root, WebView*);

  test::TaskEnvironment task_environment_;

  String base_url_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

void TouchActionTest::RunTouchActionTest(String file) {
  // runTouchActionTest() loads a document in a frame, setting up a
  // nested run loop. Should any Oilpan GC happen while it is in
  // effect, the implicit assumption that we're outside any event
  // loop (=> there being no pointers on the stack needing scanning)
  // when that GC strikes will no longer hold.
  //
  // To ensure that the references on the stack are also traced, we
  // turn them into persistent, stack allocated references. This
  // workaround is sufficient to handle this artificial test
  // scenario.
  WebViewImpl* web_view = SetupTest(file);

  Persistent<Document> document =
      static_cast<Document*>(web_view->MainFrameImpl()->GetDocument());
  RunTestOnTree(document.Get(), web_view);

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

void TouchActionTest::RunShadowDOMTest(String file) {
  WebViewImpl* web_view = SetupTest(file);

  DummyExceptionStateForTesting es;

  // Oilpan: see runTouchActionTest() comment why these are persistent
  // references.
  Persistent<Document> document =
      static_cast<Document*>(web_view->MainFrameImpl()->GetDocument());
  Persistent<StaticElementList> host_nodes =
      document->QuerySelectorAll(AtomicString("[shadow-host]"), es);
  ASSERT_FALSE(es.HadException());
  ASSERT_GE(host_nodes->length(), 1u);

  for (unsigned index = 0; index < host_nodes->length(); index++) {
    ShadowRoot* shadow_root = host_nodes->item(index)->OpenShadowRoot();
    RunTestOnTree(shadow_root, web_view);
  }

  // Projections show up in the main document.
  RunTestOnTree(document.Get(), web_view);

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

void TouchActionTest::RunIFrameTest(String file) {
  WebViewImpl* web_view = SetupTest(file);
  WebFrame* cur_frame = web_view->MainFrame()->FirstChild();
  ASSERT_TRUE(cur_frame);

  for (; cur_frame; cur_frame = cur_frame->NextSibling()) {
    // Oilpan: see runTouchActionTest() comment why these are persistent
    // references.
    Persistent<Document> content_doc =
        static_cast<Document*>(cur_frame->ToWebLocalFrame()->GetDocument());
    RunTestOnTree(content_doc.Get(), web_view);
  }

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

WebViewImpl* TouchActionTest::SetupTest(String file) {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |web_view_helper_|.
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString(base_url_), test::CoreTestDataPath(), WebString(file));
  // Note that JavaScript must be enabled for shadow DOM tests.
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_.Utf8() + file.Utf8(), nullptr, nullptr);

  // Set size to enable hit testing, and avoid line wrapping for consistency
  // with browser.
  web_view->MainFrameViewWidget()->Resize(gfx::Size(900, 1600));
  web_view->MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kAll,
                                               DocumentUpdateReason::kTest);

  // Scroll to verify the code properly transforms windows to client co-ords.
  const int kScrollOffset = 100;
  Document* document =
      static_cast<Document*>(web_view->MainFrameImpl()->GetDocument());
  document->GetFrame()->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kScrollOffset), mojom::blink::ScrollType::kProgrammatic);

  return web_view;
}

gfx::Rect WindowClipRect(const LocalFrameView& frame_view) {
  PhysicalRect clip_rect(PhysicalOffset(), PhysicalSize(frame_view.Size()));
  frame_view.GetLayoutView()->MapToVisualRectInAncestorSpace(nullptr,
                                                             clip_rect);
  return ToEnclosingRect(clip_rect);
}

void TouchActionTest::RunTestOnTree(ContainerNode* root, WebView* web_view) {
  // Find all elements to test the touch-action of in the document.
  DummyExceptionStateForTesting es;

  // Oilpan: see runTouchActionTest() comment why these are persistent
  // references.
  Persistent<StaticElementList> elements =
      root->QuerySelectorAll(AtomicString("[expected-action]"), es);
  ASSERT_FALSE(es.HadException());

  for (unsigned index = 0; index < elements->length(); index++) {
    Element* element = elements->item(index);
    element->scrollIntoViewIfNeeded();

    StringBuilder failure_context;
    failure_context.Append("Test case: ");
    if (element->HasID()) {
      failure_context.Append(element->GetIdAttribute());
    } else if (element->firstChild()) {
      failure_context.Append("\"");
      failure_context.Append(element->firstChild()
                                 ->textContent(false)
                                 .StripWhiteSpace()
                                 .Ascii()
                                 .data());
      failure_context.Append("\"");
    } else {
      failure_context.Append("<missing ID>");
    }

    // Run each test three times at different positions in the element.
    // Note that we don't want the bounding box because our tests sometimes have
    // elements with multiple border boxes with other elements in between. Use
    // the first border box (which we can easily visualize in a browser for
    // debugging).
    Persistent<DOMRectList> rects = element->getClientRects();
    ASSERT_GE(rects->length(), 0u) << failure_context.ToString();
    if (!rects->length())
      continue;
    Persistent<DOMRect> r = rects->item(0);
    gfx::RectF client_float_rect =
        gfx::RectF(r->left(), r->top(), r->width(), r->height());
    gfx::Rect client_rect = ToEnclosedRect(client_float_rect);
    for (int loc_idx = 0; loc_idx < 3; loc_idx++) {
      gfx::Point frame_point;
      std::stringstream context_stream;
      context_stream << failure_context.ToString() << " (";
      switch (loc_idx) {
        case 0:
          frame_point = client_rect.CenterPoint();
          context_stream << "center";
          break;
        case 1:
          frame_point = client_rect.origin();
          context_stream << "top-left";
          break;
        case 2:
          frame_point = client_rect.bottom_right();
          frame_point.Offset(-1, -1);
          context_stream << "bottom-right";
          break;
        default:
          FAIL() << "Invalid location index.";
      }

      gfx::Point window_point =
          root->GetDocument().GetFrame()->View()->ConvertToRootFrame(
              frame_point);
      context_stream << "=" << window_point.x() << "," << window_point.y()
                     << ").";
      String failure_context_pos = String::FromUTF8(context_stream.str());

      LocalFrame* main_frame =
          To<LocalFrame>(WebFrame::ToCoreFrame(*web_view->MainFrame()));
      LocalFrameView* main_frame_view = main_frame->View();
      gfx::Rect visible_rect = WindowClipRect(*main_frame_view);
      ASSERT_TRUE(visible_rect.Contains(window_point))
          << failure_context_pos
          << " Test point not contained in visible area: " << visible_rect.x()
          << "," << visible_rect.y() << "-" << visible_rect.right() << ","
          << visible_rect.bottom();

      // First validate that a hit test at this point will really hit the
      // element we intended. This is the easiest way for a test to be broken,
      // but has nothing really to do with touch action.  Note that we can't use
      // WebView's hit test API because it doesn't look into shadow DOM.
      HitTestLocation location(window_point);
      HitTestResult result =
          main_frame->GetEventHandler().HitTestResultAtLocation(
              location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
      ASSERT_EQ(element, result.InnerElement())
          << "Unexpected hit test result " << failure_context_pos
          << "  Got element: \""
          << result.InnerElement()
                 ->outerHTML()
                 .StripWhiteSpace()
                 .Left(80)
                 .Ascii()
                 .data()
          << "\"" << std::endl
          << "Document render tree:" << std::endl
          << ExternalRepresentation(root->GetDocument().GetFrame()).Utf8();

      // Now send the touch event and check any touch action result.
      SendTouchEvent(web_view, WebInputEvent::Type::kPointerDown, window_point);
      RunPendingTasks();

      TouchActionTrackingWebFrameWidget* widget =
          static_cast<TouchActionTrackingWebFrameWidget*>(
              web_view->MainFrameWidget());

      AtomicString expected_action =
          element->getAttribute(AtomicString("expected-action"));
      // Should have received exactly one touch action, even for auto.
      EXPECT_EQ(1, widget->TouchActionSetCount()) << failure_context_pos;
      if (widget->TouchActionSetCount()) {
        if (expected_action == "auto") {
          EXPECT_EQ(TouchAction::kAuto, widget->LastTouchAction())
              << failure_context_pos;
        } else if (expected_action == "none") {
          EXPECT_EQ(TouchAction::kNone, widget->LastTouchAction() &
                                            ~TouchAction::kInternalNotWritable)
              << failure_context_pos;
        } else if (expected_action == "pan-x") {
          EXPECT_EQ(TouchAction::kPanX, widget->LastTouchAction() &
                                            ~TouchAction::kInternalPanXScrolls &
                                            ~TouchAction::kInternalNotWritable)
              << failure_context_pos;
        } else if (expected_action == "pan-y") {
          EXPECT_EQ(TouchAction::kPanY, widget->LastTouchAction() &
                                            ~TouchAction::kInternalNotWritable)
              << failure_context_pos;
        } else if (expected_action == "pan-x-y") {
          EXPECT_EQ(TouchAction::kPan, widget->LastTouchAction() &
                                           ~TouchAction::kInternalPanXScrolls &
                                           ~TouchAction::kInternalNotWritable)
              << failure_context_pos;
        } else if (expected_action == "manipulation") {
          EXPECT_EQ(TouchAction::kManipulation,
                    widget->LastTouchAction() &
                        ~TouchAction::kInternalPanXScrolls &
                        ~TouchAction::kInternalNotWritable)
              << failure_context_pos;
        } else {
          FAIL() << "Unrecognized expected-action " << expected_action << " "
                 << failure_context_pos;
        }
      }

      // Reset webview touch state.
      widget->Reset();
      SendTouchEvent(web_view, WebInputEvent::Type::kPointerCancel,
                     window_point);
      EXPECT_EQ(0, widget->TouchActionSetCount());
    }
  }
}
void TouchActionTest::SendTouchEvent(WebView* web_view,
                                     WebInputEvent::Type type,
                                     gfx::Point client_point) {
  ASSERT_TRUE(type == WebInputEvent::Type::kPointerDown ||
              type == WebInputEvent::Type::kPointerCancel);

  WebPointerEvent event(
      type,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(client_point.x(), client_point.y()),
                           gfx::PointF(client_point.x(), client_point.y())),
      10.0f, 10.0f);
  if (type == WebInputEvent::Type::kPointerCancel)
    event.dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
  else
    event.touch_start_or_first_touch_move = true;

  web_view->MainFrameWidget()->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();
  RunPendingTasks();
}

// crbug.com/411038
TEST_F(TouchActionTest, Simple) {
  RunTouchActionTest("touch-action-simple.html");
}

TEST_F(TouchActionTest, Overflow) {
  RunTouchActionTest("touch-action-overflow.html");
}

TEST_F(TouchActionTest, IFrame) {
  RunIFrameTest("touch-action-iframe.html");
}

TEST_F(TouchActionTest, ShadowDOM) {
  RunShadowDOMTest("touch-action-shadow-dom.html");
}

TEST_F(TouchActionTest, Pan) {
  RunTouchActionTest("touch-action-pan.html");
}

}  // namespace blink
