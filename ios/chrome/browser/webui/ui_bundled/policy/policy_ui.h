// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_POLICY_POLICY_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_POLICY_POLICY_UI_H_

#include <string>

#import "base/values.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The Web UI controller for the chrome://policy page.
class PolicyUI : public web::WebUIIOSController {
 public:
  explicit PolicyUI(web::WebUIIOS* web_ui, const std::string& host);
  ~PolicyUI() override;
  PolicyUI(const PolicyUI&) = delete;
  PolicyUI& operator=(const PolicyUI&) = delete;

  static bool ShouldLoadTestPage(ProfileIOS* profile);
  static base::Value GetSchema(ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_POLICY_POLICY_UI_H_
