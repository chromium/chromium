// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FAVICON_FAVICON_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_FAVICON_FAVICON_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

class FaviconJavaScriptFeature : public JavaScriptFeature {
 public:
  FaviconJavaScriptFeature();
  ~FaviconJavaScriptFeature() override;

 private:
  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  FaviconJavaScriptFeature(const FaviconJavaScriptFeature&) = delete;
  FaviconJavaScriptFeature& operator=(const FaviconJavaScriptFeature&) = delete;
};

}  // namespace web

#endif  // IOS_WEB_FAVICON_FAVICON_JAVA_SCRIPT_FEATURE_H_
