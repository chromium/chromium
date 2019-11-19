// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/renderer/compositor/layer_tree_view.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_view.h"

namespace test_runner {

void WebViewTestProxy::Initialize(WebTestInterfaces* interfaces,
                                  std::unique_ptr<WebTestDelegate> delegate) {
  delegate_ = std::move(delegate);
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces()->WindowOpened(this);
}

blink::WebView* WebViewTestProxy::CreateView(
    blink::WebLocalFrame* creator,
    const blink::WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const blink::WebString& frame_name,
    blink::WebNavigationPolicy policy,
    blink::WebSandboxFlags sandbox_flags,
    const blink::FeaturePolicy::FeatureState& opener_feature_state,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
  if (GetTestRunner()->ShouldDumpNavigationPolicy()) {
    delegate()->PrintMessage("Default policy for createView for '" +
                             URLDescription(request.Url()) + "' is '" +
                             WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!GetTestRunner()->CanOpenWindows())
    return nullptr;

  if (GetTestRunner()->ShouldDumpCreateView()) {
    delegate()->PrintMessage(std::string("createView(") +
                             URLDescription(request.Url()) + ")\n");
  }
  return RenderViewImpl::CreateView(creator, request, features, frame_name,
                                    policy, sandbox_flags, opener_feature_state,
                                    session_storage_namespace_id);
}

void WebViewTestProxy::PrintPage(blink::WebLocalFrame* frame) {
  blink::WebSize page_size_in_pixels = GetWidget()->GetWebWidget()->Size();
  if (page_size_in_pixels.IsEmpty())
    return;
  blink::WebPrintParams print_params(page_size_in_pixels);
  frame->PrintBegin(print_params);
  frame->PrintEnd();
}

blink::WebString WebViewTestProxy::AcceptLanguages() {
  return blink::WebString::FromUTF8(GetTestRunner()->GetAcceptLanguages());
}

void WebViewTestProxy::DidFocus(blink::WebLocalFrame* calling_frame) {
  GetTestRunner()->SetFocus(webview(), true);
  RenderViewImpl::DidFocus(calling_frame);
}

void WebViewTestProxy::Reset() {
  // TODO(https://crbug.com/961499): There is a race condition where Reset()
  // can be called after GetWidget() has been nulled, but before this is
  // destructed.
  if (!GetWidget())
    return;
  accessibility_controller_.Reset();
  // text_input_controller_ doesn't have any state to reset.
  view_test_runner_.Reset();
  static_cast<WebWidgetTestProxy*>(GetWidget())->Reset();

  for (blink::WebFrame* frame = webview()->MainFrame(); frame;
       frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame())
      delegate_->GetWebWidgetTestProxy(frame->ToWebLocalFrame())->Reset();
  }
}

void WebViewTestProxy::BindTo(blink::WebLocalFrame* frame) {
  accessibility_controller_.Install(frame);
  text_input_controller_.Install(frame);
  view_test_runner_.Install(frame);
}

WebViewTestProxy::~WebViewTestProxy() {
  test_interfaces_->WindowClosed(this);
  if (test_interfaces_->GetDelegate() == delegate_.get()) {
    test_interfaces_->SetDelegate(nullptr);
    test_interfaces_->SetMainView(nullptr);
  }
}

TestRunner* WebViewTestProxy::GetTestRunner() {
  return test_interfaces()->GetTestRunner();
}

}  // namespace test_runner
