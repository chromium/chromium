// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_ERROR_EVENT_LISTENER_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_ERROR_EVENT_LISTENER_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// This feature injects a script which listens for JavaScript errors. Errors are
// reported back to the native application through the
// `WindowErrorResultHandler` message handler registered by
// `WindowErrorJavaScriptFeature`.
class ErrorEventListenerJavaScriptFeature : public JavaScriptFeature {
 public:
  static ErrorEventListenerJavaScriptFeature* GetInstance();

  ErrorEventListenerJavaScriptFeature(
      const ErrorEventListenerJavaScriptFeature&) = delete;
  ErrorEventListenerJavaScriptFeature& operator=(
      const ErrorEventListenerJavaScriptFeature&) = delete;

 private:
  friend class base::NoDestructor<ErrorEventListenerJavaScriptFeature>;

  ErrorEventListenerJavaScriptFeature();
  ~ErrorEventListenerJavaScriptFeature() override;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_ERROR_EVENT_LISTENER_JAVA_SCRIPT_FEATURE_H_
