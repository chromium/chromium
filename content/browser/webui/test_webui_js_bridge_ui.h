// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_TEST_WEBUI_JS_BRIDGE_UI_H_
#define CONTENT_BROWSER_WEBUI_TEST_WEBUI_JS_BRIDGE_UI_H_

#include "content/public/browser/web_ui_controller.h"

#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace content {

const static constexpr char kTestWebUIJsBridgeUIUrl[] = "chrome://foo";
const static constexpr char kTestWebUIJsBridgeUIHost[] = "foo";

// WebUIController for WebUIJsBridge unittests.
class TestWebUIJsBridgeUI : public WebUIController {
 public:
  explicit TestWebUIJsBridgeUI(WebUI* web_ui);
  ~TestWebUIJsBridgeUI() override = default;

  TestWebUIJsBridgeUI(const TestWebUIJsBridgeUI&) = delete;
  TestWebUIJsBridgeUI& operator=(const TestWebUIJsBridgeUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for TestWebUIJsBridgeUI.
class TestWebUIJsBridgeUIConfig
    : public DefaultWebUIConfig<TestWebUIJsBridgeUI> {
 public:
  explicit TestWebUIJsBridgeUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kTestWebUIJsBridgeUIHost) {}
};

// Additional WebUIController for WebUIJsBridge unittests.
class TestWebUIJsBridgeUI2 : public WebUIController {
 public:
  explicit TestWebUIJsBridgeUI2(WebUI* web_ui) : WebUIController(web_ui) {}
  ~TestWebUIJsBridgeUI2() override = default;

  TestWebUIJsBridgeUI2(const TestWebUIJsBridgeUI2&) = delete;
  TestWebUIJsBridgeUI2& operator=(const TestWebUIJsBridgeUI2&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_TEST_WEBUI_JS_BRIDGE_UI_H_
