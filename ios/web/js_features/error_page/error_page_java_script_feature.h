// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_ERROR_PAGE_ERROR_PAGE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_ERROR_PAGE_ERROR_PAGE_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// Listens for script command messages from error pages.
class ErrorPageJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static ErrorPageJavaScriptFeature* GetInstance();

  ErrorPageJavaScriptFeature(const ErrorPageJavaScriptFeature&) = delete;
  ErrorPageJavaScriptFeature& operator=(const ErrorPageJavaScriptFeature&) =
      delete;

 private:
  friend class base::NoDestructor<ErrorPageJavaScriptFeature>;

  // JavaScriptFeature overrides
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& script_message) override;

  ErrorPageJavaScriptFeature();
  ~ErrorPageJavaScriptFeature() override;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_ERROR_PAGE_ERROR_PAGE_JAVA_SCRIPT_FEATURE_H_
