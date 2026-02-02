// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/functional/callback.h"
#import "ios/web/js_features/window_error/script_error_details.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// A feature which listens for `WindowErrorResultHandler` script messages and
// executes the given callback with details about each received error.
class ScriptErrorMessageHandlerJavaScriptFeature : public JavaScriptFeature {
 public:
  ScriptErrorMessageHandlerJavaScriptFeature(
      base::RepeatingCallback<void(ScriptErrorDetails)> callback);
  ~ScriptErrorMessageHandlerJavaScriptFeature() override;

  ScriptErrorMessageHandlerJavaScriptFeature(
      const ScriptErrorMessageHandlerJavaScriptFeature&) = delete;
  ScriptErrorMessageHandlerJavaScriptFeature& operator=(
      const ScriptErrorMessageHandlerJavaScriptFeature&) = delete;

 private:
  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;

  base::RepeatingCallback<void(ScriptErrorDetails)> callback_;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_
