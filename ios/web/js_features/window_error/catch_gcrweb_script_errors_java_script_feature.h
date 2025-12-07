// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_CATCH_GCRWEB_SCRIPT_ERRORS_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_CATCH_GCRWEB_SCRIPT_ERRORS_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// This feature injects a script that wraps every function exposed on
// `__gCrWeb.{any_namespace}` in a try-catch block and reports the errors back
// to the native application through `WindowErrorJavaScriptFeature`s
// message handler: `WindowErrorResultHandler`.
class CatchGCrWebScriptErrorsJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static CatchGCrWebScriptErrorsJavaScriptFeature* GetInstance();

  CatchGCrWebScriptErrorsJavaScriptFeature(
      const CatchGCrWebScriptErrorsJavaScriptFeature&) = delete;
  CatchGCrWebScriptErrorsJavaScriptFeature& operator=(
      const CatchGCrWebScriptErrorsJavaScriptFeature&) = delete;

 private:
  friend class base::NoDestructor<CatchGCrWebScriptErrorsJavaScriptFeature>;

  CatchGCrWebScriptErrorsJavaScriptFeature();
  ~CatchGCrWebScriptErrorsJavaScriptFeature() override;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_CATCH_GCRWEB_SCRIPT_ERRORS_JAVA_SCRIPT_FEATURE_H_
