// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_

#include "ios/web/public/js_messaging/java_script_feature.h"

// A JavaScriptFeature that detects key presses and paste actions in the web
// content area.
class PasswordProtectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  PasswordProtectionJavaScriptFeature();
  ~PasswordProtectionJavaScriptFeature() override;

  // This feature holds no state, so only a single static instance is ever
  // needed.
  static PasswordProtectionJavaScriptFeature* GetInstance();

  // JavaScriptFeature:
  base::Optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::BrowserState* browser_state,
                             WKScriptMessage* message) override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_
