// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_INTERFACES_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_INTERFACES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/shell/test_runner/mock_web_theme_engine.h"

namespace blink {
class WebLocalFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace test_runner {

class GamepadController;
class TestRunner;
class WebTestDelegate;
class WebViewTestProxyBase;

class TestInterfaces {
 public:
  TestInterfaces();
  ~TestInterfaces();

  void SetMainView(blink::WebView* web_view);
  void SetDelegate(WebTestDelegate* delegate);
  void BindTo(blink::WebLocalFrame* frame);
  void ResetTestHelperControllers();
  void ResetAll();
  bool TestIsRunning();
  void SetTestIsRunning(bool running);
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool protocol_mode);

  void WindowOpened(WebViewTestProxyBase* proxy);
  void WindowClosed(WebViewTestProxyBase* proxy);

  TestRunner* GetTestRunner();
  WebTestDelegate* GetDelegate();
  const std::vector<WebViewTestProxyBase*>& GetWindowList();
  blink::WebThemeEngine* GetThemeEngine();

 private:
  std::unique_ptr<GamepadController> gamepad_controller_;
  std::unique_ptr<TestRunner> test_runner_;
  WebTestDelegate* delegate_;

  std::vector<WebViewTestProxyBase*> window_list_;
  blink::WebView* main_view_;

  std::unique_ptr<MockWebThemeEngine> theme_engine_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_INTERFACES_H_
