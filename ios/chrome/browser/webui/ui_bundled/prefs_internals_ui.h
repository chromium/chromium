// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PREFS_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PREFS_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUIController for chrome://prefs-internals. Renders the current user
// prefs.
class PrefsInternalsUI : public web::WebUIIOSController {
 public:
  explicit PrefsInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  PrefsInternalsUI(const PrefsInternalsUI&) = delete;
  PrefsInternalsUI& operator=(const PrefsInternalsUI&) = delete;

  ~PrefsInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PREFS_INTERNALS_UI_H_
