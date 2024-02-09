// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_IOS_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_IOS_H_

#include <string>

#import "ios/web/public/webui/web_ui_ios_controller.h"

// The implementation for the chrome://password-manager-internals page.
class PasswordManagerInternalsUIIOS : public web::WebUIIOSController {
 public:
  explicit PasswordManagerInternalsUIIOS(web::WebUIIOS* web_ui,
                                         const std::string& host);

  PasswordManagerInternalsUIIOS(const PasswordManagerInternalsUIIOS&) = delete;
  PasswordManagerInternalsUIIOS& operator=(
      const PasswordManagerInternalsUIIOS&) = delete;

  ~PasswordManagerInternalsUIIOS() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_IOS_H_
