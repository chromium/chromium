// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A feature which the clicks on Choose file input and logs data about it.
class ChooseFileJavaScriptFeature : public web::JavaScriptFeature {
 public:
  ChooseFileJavaScriptFeature();
  ~ChooseFileJavaScriptFeature() override;

  // This feature holds no state. Thus, a single static instance
  // suffices.
  static ChooseFileJavaScriptFeature* GetInstance();

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Logs the click on choose file input.
  void LogChooseFileEvent(int accept_type, bool allow_multiple_files);

  // Returns and resets `last_choose_file_event_`.
  std::optional<ChooseFileEvent> ResetLastChooseFileEvent();

 private:
  // Latest `ChooseFileEvent` received from JavaScript.
  std::optional<ChooseFileEvent> last_choose_file_event_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_JAVA_SCRIPT_FEATURE_H_
