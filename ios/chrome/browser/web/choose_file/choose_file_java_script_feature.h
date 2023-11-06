// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_CHOOSE_FILE_CHOOSE_FILE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_CHOOSE_FILE_CHOOSE_FILE_JAVA_SCRIPT_FEATURE_H_

#include "ios/web/public/js_messaging/java_script_feature.h"

// A feature which the clicks on Choose file input and logs data about it.
class ChooseFileJavaScriptFeature : public web::JavaScriptFeature {
 public:
  ChooseFileJavaScriptFeature();
  ~ChooseFileJavaScriptFeature() override;

  // This feature holds no state. Thus, a single static instance
  // suffices.
  static ChooseFileJavaScriptFeature* GetInstance();

  // JavaScriptFeature:
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Logs the click on choose file input.
  void LogChooseFileEvent(int accept_type, bool allow_multiple_files);
};

#endif  // IOS_CHROME_BROWSER_WEB_CHOOSE_FILE_CHOOSE_FILE_JAVA_SCRIPT_FEATURE_H_
