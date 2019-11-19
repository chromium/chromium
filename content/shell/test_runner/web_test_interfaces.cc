// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_test_interfaces.h"

#include <utility>

#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"

namespace test_runner {

WebTestInterfaces::WebTestInterfaces() : interfaces_(new TestInterfaces()) {}

WebTestInterfaces::~WebTestInterfaces() {}

void WebTestInterfaces::SetMainView(blink::WebView* web_view) {
  interfaces_->SetMainView(web_view);
}

void WebTestInterfaces::SetDelegate(WebTestDelegate* delegate) {
  interfaces_->SetDelegate(delegate);
}

void WebTestInterfaces::ResetAll() {
  interfaces_->ResetAll();
}

bool WebTestInterfaces::TestIsRunning() {
  return interfaces_->TestIsRunning();
}

void WebTestInterfaces::SetTestIsRunning(bool running) {
  interfaces_->SetTestIsRunning(running);
}

void WebTestInterfaces::ConfigureForTestWithURL(const blink::WebURL& test_url,
                                                bool protocol_mode) {
  interfaces_->ConfigureForTestWithURL(test_url, protocol_mode);
}

WebTestRunner* WebTestInterfaces::TestRunner() {
  return interfaces_->GetTestRunner();
}

TestInterfaces* WebTestInterfaces::GetTestInterfaces() {
  return interfaces_.get();
}

std::unique_ptr<WebFrameTestClient> WebTestInterfaces::CreateWebFrameTestClient(
    WebViewTestProxy* web_view_test_proxy,
    WebFrameTestProxy* web_frame_test_proxy) {
  // TODO(lukasza): Do not pass the WebTestDelegate below - it's lifetime can
  // differ from the lifetime of WebFrameTestClient - https://crbug.com/606594.
  return std::make_unique<WebFrameTestClient>(
      interfaces_->GetDelegate(), web_view_test_proxy, web_frame_test_proxy);
}

std::vector<blink::WebView*> WebTestInterfaces::GetWindowList() {
  std::vector<blink::WebView*> result;
  for (WebViewTestProxy* proxy : interfaces_->GetWindowList())
    result.push_back(proxy->webview());
  return result;
}

}  // namespace test_runner
