// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_TEST_INTERFACES_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_TEST_INTERFACES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/shell/test_runner/test_runner_export.h"

namespace blink {
class WebLocalFrameClient;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebURL;
class WebView;
}

namespace test_runner {

class TestInterfaces;
class WebFrameTestClient;
class WebFrameTestProxy;
class WebTestDelegate;
class WebViewTestProxy;
class WebTestRunner;

class TEST_RUNNER_EXPORT WebTestInterfaces {
 public:
  WebTestInterfaces();
  ~WebTestInterfaces();

  void SetMainView(blink::WebView* web_view);
  void SetDelegate(WebTestDelegate* delegate);
  void ResetAll();
  bool TestIsRunning();
  void SetTestIsRunning(bool running);

  // Configures the renderer for the test, based on |test_url| and
  // |procotol_mode|.
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool protocol_mode);

  WebTestRunner* TestRunner();

  std::unique_ptr<blink::WebRTCPeerConnectionHandler>
  CreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);

  TestInterfaces* GetTestInterfaces();

  // Creates a WebLocalFrameClient implementation providing test behavior (i.e.
  // forwarding javascript console output to the test harness).  The caller
  // should guarantee that the returned object won't be used beyond the lifetime
  // of WebTestInterfaces and/or the lifetime of |web_view_test_proxy|.
  std::unique_ptr<WebFrameTestClient> CreateWebFrameTestClient(
      WebViewTestProxy* web_view_test_proxy,
      WebFrameTestProxy* web_frame_test_proxy);

  // Gets a list of currently opened windows created by the current test.
  std::vector<blink::WebView*> GetWindowList();

 private:
  std::unique_ptr<TestInterfaces> interfaces_;

  DISALLOW_COPY_AND_ASSIGN(WebTestInterfaces);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_TEST_INTERFACES_H_
