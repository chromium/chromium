// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using blink::frame_test_helpers::LoadFrame;
using blink::test::RunPendingTasks;
using blink::url_test_helpers::RegisterMockedURLLoadFromBase;

namespace blink {

class ImeRequestTrackingWebWidgetClient
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  ImeRequestTrackingWebWidgetClient() : virtual_keyboard_request_count_(0) {}

  // WebWidgetClient methods
  void ShowVirtualKeyboardOnElementFocus() override {
    ++virtual_keyboard_request_count_;
  }

  // Local methds
  void Reset() { virtual_keyboard_request_count_ = 0; }

  int VirtualKeyboardRequestCount() { return virtual_keyboard_request_count_; }

 private:
  int virtual_keyboard_request_count_;
};

class ImeOnFocusTest : public testing::Test {
 public:
  ImeOnFocusTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void SendGestureTap(WebView*, IntPoint);
  void Focus(const AtomicString& element);
  void RunImeOnFocusTest(String file_name,
                         int,
                         IntPoint tap_point = IntPoint(-1, -1),
                         const AtomicString& focus_element = g_null_atom,
                         String frame = "");

  String base_url_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  Persistent<Document> document_;
};

void ImeOnFocusTest::SendGestureTap(WebView* web_view, IntPoint client_point) {
  WebGestureEvent web_gesture_event(WebInputEvent::kGestureTap,
                                    WebInputEvent::kNoModifiers,
                                    WebInputEvent::GetStaticTimeStampForTests(),
                                    WebGestureDevice::kTouchscreen);
  // GestureTap is only ever from touch screens.
  web_gesture_event.SetPositionInWidget(FloatPoint(client_point));
  web_gesture_event.SetPositionInScreen(FloatPoint(client_point));
  web_gesture_event.data.tap.tap_count = 1;
  web_gesture_event.data.tap.width = 10;
  web_gesture_event.data.tap.height = 10;

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(web_gesture_event));
  RunPendingTasks();
}

void ImeOnFocusTest::Focus(const AtomicString& element) {
  document_->body()->getElementById(element)->focus();
}

void ImeOnFocusTest::RunImeOnFocusTest(
    String file_name,
    int expected_virtual_keyboard_request_count,
    IntPoint tap_point,
    const AtomicString& focus_element,
    String frame) {
  ImeRequestTrackingWebWidgetClient client;
  RegisterMockedURLLoadFromBase(WebString(base_url_), test::CoreTestDataPath(),
                                WebString(file_name));
  WebViewImpl* web_view =
      web_view_helper_.Initialize(nullptr, nullptr, &client);
  web_view->MainFrameWidget()->Resize(WebSize(800, 1200));
  LoadFrame(web_view->MainFrameImpl(), base_url_.Utf8() + file_name.Utf8());
  document_ = web_view_helper_.GetWebView()
                  ->MainFrameImpl()
                  ->GetDocument()
                  .Unwrap<Document>();

  if (!focus_element.IsNull())
    Focus(focus_element);
  EXPECT_EQ(0, client.VirtualKeyboardRequestCount());

  if (tap_point.X() >= 0 && tap_point.Y() >= 0)
    SendGestureTap(web_view, tap_point);

  if (!frame.IsEmpty()) {
    RegisterMockedURLLoadFromBase(WebString(base_url_),
                                  test::CoreTestDataPath(), WebString(frame));
    WebLocalFrame* child_frame =
        web_view->MainFrame()->FirstChild()->ToWebLocalFrame();
    LoadFrame(child_frame, base_url_.Utf8() + frame.Utf8());
  }

  if (!focus_element.IsNull())
    Focus(focus_element);
  EXPECT_EQ(expected_virtual_keyboard_request_count,
            client.VirtualKeyboardRequestCount());

  web_view_helper_.Reset();
}

TEST_F(ImeOnFocusTest, OnLoad) {
  RunImeOnFocusTest("ime-on-focus-on-load.html", 0);
}

TEST_F(ImeOnFocusTest, OnAutofocus) {
  RunImeOnFocusTest("ime-on-focus-on-autofocus.html", 0);
}

TEST_F(ImeOnFocusTest, OnUserGesture) {
  RunImeOnFocusTest("ime-on-focus-on-user-gesture.html", 1, IntPoint(50, 50));
}

TEST_F(ImeOnFocusTest, AfterFirstGesture) {
  RunImeOnFocusTest("ime-on-focus-after-first-gesture.html", 1,
                    IntPoint(50, 50), "input");
}

TEST_F(ImeOnFocusTest, AfterNavigationWithinPage) {
  RunImeOnFocusTest("ime-on-focus-after-navigation-within-page.html", 1,
                    IntPoint(50, 50), "input");
}

TEST_F(ImeOnFocusTest, AfterFrameLoadOnGesture) {
  RunImeOnFocusTest("ime-on-focus-after-frame-load-on-gesture.html", 1,
                    IntPoint(50, 50), "input", "frame.html");
}

}  // namespace blink
