// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_JS_MESSAGING_WEB_VIEW_SCRIPTS_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_VIEW_INTERNAL_JS_MESSAGING_WEB_VIEW_SCRIPTS_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/supports_user_data.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class BrowserState;
}  // namespace web

class WebViewScriptsJavaScriptFeature : public base::SupportsUserData::Data,
                                        public web::JavaScriptFeature {
 public:
  WebViewScriptsJavaScriptFeature(web::BrowserState* browser_state);
  ~WebViewScriptsJavaScriptFeature() override;

  WebViewScriptsJavaScriptFeature(const WebViewScriptsJavaScriptFeature&) =
      delete;
  WebViewScriptsJavaScriptFeature& operator=(
      const WebViewScriptsJavaScriptFeature&) = delete;

  // Returns the WebViewScriptsJavaScriptFeature associated with
  // `browser_state`, creating one if necessary. `browser_state` must not be
  // null.
  static WebViewScriptsJavaScriptFeature* FromBrowserState(
      web::BrowserState* browser_state);

  void SetScripts(std::optional<std::string> all_frames_script,
                  std::optional<std::string> main_frame_script);

 private:
  std::vector<FeatureScript> GetScripts() const override;

  // The browser state associated with this feature.
  web::BrowserState* browser_state_;

  std::optional<std::string> all_frames_script_;
  std::optional<std::string> main_frame_script_;
};

#endif  // IOS_WEB_VIEW_INTERNAL_JS_MESSAGING_WEB_VIEW_SCRIPTS_JAVA_SCRIPT_FEATURE_H_
