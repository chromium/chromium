// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_view_test_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace test_runner {

WebViewTestClient::WebViewTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base,
    std::unique_ptr<blink::WebWidgetClient> web_widget_client)
    : web_view_test_proxy_base_(web_view_test_proxy_base),
      web_widget_client_(std::move(web_widget_client)) {
  DCHECK(web_view_test_proxy_base);
}

WebViewTestClient::~WebViewTestClient() {}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

blink::WebView* WebViewTestClient::CreateView(
    blink::WebLocalFrame* frame,
    const blink::WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const blink::WebString& frame_name,
    blink::WebNavigationPolicy policy,
    bool suppress_opener,
    blink::WebSandboxFlags sandbox_flags,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  if (test_runner()->shouldDumpNavigationPolicy()) {
    delegate()->PrintMessage("Default policy for createView for '" +
                             URLDescription(request.Url()) + "' is '" +
                             WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!test_runner()->canOpenWindows())
    return nullptr;
  if (test_runner()->shouldDumpCreateView())
    delegate()->PrintMessage(std::string("createView(") +
                             URLDescription(request.Url()) + ")\n");

  // The return value below is used to communicate to WebViewTestProxy whether
  // it should forward the createView request to RenderViewImpl or not.  The
  // somewhat ugly cast is used to do this while fitting into the existing
  // WebViewClient interface.
  return reinterpret_cast<blink::WebView*>(0xdeadbeef);
}

// Simulate a print by going into print mode and then exit straight away.
void WebViewTestClient::PrintPage(blink::WebLocalFrame* frame) {
  blink::WebSize page_size_in_pixels = frame->View()->Size();
  if (page_size_in_pixels.IsEmpty())
    return;
  blink::WebPrintParams printParams(page_size_in_pixels);
  frame->PrintBegin(printParams);
  frame->PrintEnd();
}

blink::WebString WebViewTestClient::AcceptLanguages() {
  return blink::WebString::FromUTF8(test_runner()->GetAcceptLanguages());
}

WebTestDelegate* WebViewTestClient::delegate() {
  return web_view_test_proxy_base_->delegate();
}

void WebViewTestClient::DidFocus(blink::WebLocalFrame* calling_frame) {
  test_runner()->SetFocus(web_view_test_proxy_base_->web_view(), true);
}

TestRunner* WebViewTestClient::test_runner() {
  return web_view_test_proxy_base_->test_interfaces()->GetTestRunner();
}

bool WebViewTestClient::CanHandleGestureEvent() {
  return true;
}

bool WebViewTestClient::CanUpdateLayout() {
  return true;
}

blink::WebScreenInfo WebViewTestClient::GetScreenInfo() {
  blink::WebScreenInfo screen_info;
  MockScreenOrientationClient* mock_client =
      test_runner()->getMockScreenOrientationClient();
  if (mock_client->IsDisabled()) {
    // Indicate to WebViewTestProxy that there is no test/mock info.
    screen_info.orientation_type = blink::kWebScreenOrientationUndefined;
  } else {
    // Override screen orientation information with mock data.
    screen_info.orientation_type = mock_client->CurrentOrientationType();
    screen_info.orientation_angle = mock_client->CurrentOrientationAngle();
  }
  return screen_info;
}

blink::WebWidgetClient* WebViewTestClient::WidgetClient() {
  return web_widget_client_.get();
}

}  // namespace test_runner
