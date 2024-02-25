// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USERDEFAULTS_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USERDEFAULTS_INTERNALS_UI_H_

#import <string>

#import "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}  // namespace web

// The WebUIController for the chrome://userdefaults-internals page on iOS.
class UserDefaultsInternalsUI : public web::WebUIIOSController {
 public:
  UserDefaultsInternalsUI(web::WebUIIOS* web_ui, const std::string& host);
  ~UserDefaultsInternalsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USERDEFAULTS_INTERNALS_UI_H_
