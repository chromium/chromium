// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INSPECT_INSPECT_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INSPECT_INSPECT_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://inspect which displays JavaScript console
// messages.
class InspectUI : public web::WebUIIOSController {
 public:
  explicit InspectUI(web::WebUIIOS* web_ui, const std::string& host);

  InspectUI(const InspectUI&) = delete;
  InspectUI& operator=(const InspectUI&) = delete;

  ~InspectUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INSPECT_INSPECT_UI_H_
