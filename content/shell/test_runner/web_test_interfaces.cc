// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_test_interfaces.h"

#include <utility>

#include "content/shell/test_runner/mock_web_midi_accessor.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "content/shell/test_runner/web_view_test_client.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_client.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/platform/modules/webmidi/web_midi_accessor.h"

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

blink::WebThemeEngine* WebTestInterfaces::ThemeEngine() {
  return interfaces_->GetThemeEngine();
}

TestInterfaces* WebTestInterfaces::GetTestInterfaces() {
  return interfaces_.get();
}

std::unique_ptr<blink::WebMIDIAccessor> WebTestInterfaces::CreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  return std::make_unique<MockWebMIDIAccessor>(client, interfaces_.get());
}

std::unique_ptr<WebFrameTestClient> WebTestInterfaces::CreateWebFrameTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base,
    WebFrameTestProxyBase* web_frame_test_proxy_base) {
  // TODO(lukasza): Do not pass the WebTestDelegate below - it's lifetime can
  // differ from the lifetime of WebFrameTestClient - https://crbug.com/606594.
  return std::make_unique<WebFrameTestClient>(interfaces_->GetDelegate(),
                                              web_view_test_proxy_base,
                                              web_frame_test_proxy_base);
}

std::unique_ptr<WebViewTestClient> WebTestInterfaces::CreateWebViewTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base,
    std::unique_ptr<WebWidgetTestClient> web_widget_test_client) {
  return std::make_unique<WebViewTestClient>(web_view_test_proxy_base,
                                             std::move(web_widget_test_client));
}

std::unique_ptr<WebWidgetTestClient>
WebTestInterfaces::CreateWebWidgetTestClient(
    WebWidgetTestProxyBase* web_widget_test_proxy_base) {
  return std::make_unique<WebWidgetTestClient>(web_widget_test_proxy_base);
}

std::vector<blink::WebView*> WebTestInterfaces::GetWindowList() {
  std::vector<blink::WebView*> result;
  for (WebViewTestProxyBase* proxy : interfaces_->GetWindowList())
    result.push_back(proxy->web_view());
  return result;
}

}  // namespace test_runner
