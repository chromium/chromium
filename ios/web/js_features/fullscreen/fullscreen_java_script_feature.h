// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_FULLSCREEN_FULLSCREEN_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_FULLSCREEN_FULLSCREEN_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

// A feature which listens for JavaScript messages about the configuration of
// the page's viewport.
class FullscreenJavaScriptFeature : public JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static FullscreenJavaScriptFeature* GetInstance();

  FullscreenJavaScriptFeature(const FullscreenJavaScriptFeature&) = delete;
  FullscreenJavaScriptFeature& operator=(const FullscreenJavaScriptFeature&) =
      delete;

 private:
  friend class base::NoDestructor<FullscreenJavaScriptFeature>;

  FullscreenJavaScriptFeature();
  ~FullscreenJavaScriptFeature() override;

  // JavaScriptFeature overrides.
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& message) override;
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_FULLSCREEN_FULLSCREEN_JAVA_SCRIPT_FEATURE_H_
