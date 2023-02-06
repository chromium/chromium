// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_TEST_WEBUI_JS_BRIDGE_UI_H_
#define CONTENT_BROWSER_WEBUI_TEST_WEBUI_JS_BRIDGE_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {

// WebUIController for WebUIJsBridge unittests.
class TestWebUIJsBridgeUI : public WebUIController {
 public:
  explicit TestWebUIJsBridgeUI() : WebUIController(nullptr) {}
  ~TestWebUIJsBridgeUI() override = default;

  TestWebUIJsBridgeUI(const TestWebUIJsBridgeUI&) = delete;
  TestWebUIJsBridgeUI& operator=(const TestWebUIJsBridgeUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIController for WebUIJsBridge unittests.
class TestWebUIJsBridgeIncorrectUI : public WebUIController {
 public:
  explicit TestWebUIJsBridgeIncorrectUI() : WebUIController(nullptr) {}
  ~TestWebUIJsBridgeIncorrectUI() override = default;

  TestWebUIJsBridgeIncorrectUI(const TestWebUIJsBridgeIncorrectUI&) = delete;
  TestWebUIJsBridgeIncorrectUI& operator=(const TestWebUIJsBridgeIncorrectUI&) =
      delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_TEST_WEBUI_JS_BRIDGE_UI_H_
