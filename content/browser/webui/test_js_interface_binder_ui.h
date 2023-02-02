// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_TEST_JS_INTERFACE_BINDER_UI_H_
#define CONTENT_BROWSER_WEBUI_TEST_JS_INTERFACE_BINDER_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {

// WebUIController for JsInterfaceBinder unittests.
class TestJsInterfaceBinderUI : public WebUIController {
 public:
  explicit TestJsInterfaceBinderUI() : WebUIController(nullptr) {}
  ~TestJsInterfaceBinderUI() override = default;

  TestJsInterfaceBinderUI(const TestJsInterfaceBinderUI&) = delete;
  TestJsInterfaceBinderUI& operator=(const TestJsInterfaceBinderUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIController for JsInterfaceBinder unittests.
class TestJsInterfaceBinderIncorrectUI : public WebUIController {
 public:
  explicit TestJsInterfaceBinderIncorrectUI() : WebUIController(nullptr) {}
  ~TestJsInterfaceBinderIncorrectUI() override = default;

  TestJsInterfaceBinderIncorrectUI(const TestJsInterfaceBinderIncorrectUI&) =
      delete;
  TestJsInterfaceBinderIncorrectUI& operator=(
      const TestJsInterfaceBinderIncorrectUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_TEST_JS_INTERFACE_BINDER_UI_H_
