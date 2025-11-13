// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_REGIONAL_CAPABILITIES_INTERNALS_REGIONAL_CAPABILITIES_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_REGIONAL_CAPABILITIES_INTERNALS_REGIONAL_CAPABILITIES_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI controller for `chrome://regional-capabilities-internals`.
class RegionalCapabilitiesInternalsUI : public web::WebUIIOSController {
 public:
  explicit RegionalCapabilitiesInternalsUI(web::WebUIIOS* web_ui,
                                           const std::string& host);

  RegionalCapabilitiesInternalsUI(const RegionalCapabilitiesInternalsUI&) =
      delete;
  RegionalCapabilitiesInternalsUI& operator=(
      const RegionalCapabilitiesInternalsUI&) = delete;

  ~RegionalCapabilitiesInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_REGIONAL_CAPABILITIES_INTERNALS_REGIONAL_CAPABILITIES_INTERNALS_UI_H_
