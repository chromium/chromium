// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_H_

#import "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

class JavaScriptConsoleFeatureDelegate;

// A feature which listens for JavaScript console messages and sends details
// about them to a JavaScriptConsoleFeatureDelegate instance.
class JavaScriptConsoleFeature : public KeyedService,
                                 public web::JavaScriptFeature {
 public:
  // Creates a feature to listen for JavaScript console messages and send
  // details about those messages to `delegate_`.
  JavaScriptConsoleFeature();
  ~JavaScriptConsoleFeature() override;

  // Sets the current delegate to `delegate`. If `delegate` is null, any current
  // delegate will be removed.
  void SetDelegate(JavaScriptConsoleFeatureDelegate* delegate);

 private:
  JavaScriptConsoleFeature(const JavaScriptConsoleFeature&) = delete;
  JavaScriptConsoleFeature& operator=(const JavaScriptConsoleFeature&) = delete;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // The delegate which receives details about the console messages.
  raw_ptr<JavaScriptConsoleFeatureDelegate> delegate_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_H_
