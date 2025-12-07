// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/functional/callback.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "url/gurl.h"

namespace web {

// A feature which listens for `WindowErrorResultHandler` script messages and
// executes the given callback with details about each received error.
class ScriptErrorMessageHandlerJavaScriptFeature : public JavaScriptFeature {
 public:
  // Wraps information about an error.
  struct ErrorDetails {
   public:
    ErrorDetails();
    ~ErrorDetails();

    // The filename in which the error occurred.
    NSString* filename = nil;

    // The line number at which the error occurred.
    int line_number = 0;

    // The error message.
    NSString* message = nil;

    // The error stack.
    NSString* stack = nil;

    // The url where the error occurred.
    GURL url;

    // Whether or not this error occurred in the main frame.
    bool is_main_frame;
  };

  ScriptErrorMessageHandlerJavaScriptFeature(
      base::RepeatingCallback<void(ErrorDetails)> callback);
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

  base::RepeatingCallback<void(ErrorDetails)> callback_;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_MESSAGE_HANDLER_JAVA_SCRIPT_FEATURE_H_
