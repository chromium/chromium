// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_UKM_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_UKM_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://ukm.
class UkmInternalsUI : public web::WebUIIOSController {
 public:
  explicit UkmInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  UkmInternalsUI(const UkmInternalsUI&) = delete;
  UkmInternalsUI& operator=(const UkmInternalsUI&) = delete;

  ~UkmInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_UKM_INTERNALS_UI_H_
