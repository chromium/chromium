// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ON_DEVICE_LLM_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ON_DEVICE_LLM_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://on-device-llm-internals.
class OnDeviceLlmInternalsUI : public web::WebUIIOSController {
 public:
  OnDeviceLlmInternalsUI(web::WebUIIOS* web_ui, const std::string& name);

  OnDeviceLlmInternalsUI(const OnDeviceLlmInternalsUI&) = delete;
  OnDeviceLlmInternalsUI& operator=(const OnDeviceLlmInternalsUI&) = delete;

  ~OnDeviceLlmInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ON_DEVICE_LLM_INTERNALS_UI_H_
