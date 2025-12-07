// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PROFILE_INTERNALS_PROFILE_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PROFILE_INTERNALS_PROFILE_INTERNALS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://profile-internals.
class ProfileInternalsUI : public web::WebUIIOSController {
 public:
  explicit ProfileInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  ProfileInternalsUI(const ProfileInternalsUI&) = delete;
  ProfileInternalsUI& operator=(const ProfileInternalsUI&) = delete;

  ~ProfileInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_PROFILE_INTERNALS_PROFILE_INTERNALS_UI_H_
