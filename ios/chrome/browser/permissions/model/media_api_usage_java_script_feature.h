// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_MEDIA_API_USAGE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_MEDIA_API_USAGE_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

// A feature which listens for usage of media APIs.
class MediaAPIUsageJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static MediaAPIUsageJavaScriptFeature* GetInstance();

  static bool ShouldOverrideAPI();

  MediaAPIUsageJavaScriptFeature();
  ~MediaAPIUsageJavaScriptFeature() override;

  MediaAPIUsageJavaScriptFeature(const MediaAPIUsageJavaScriptFeature&) =
      delete;
  MediaAPIUsageJavaScriptFeature& operator=(
      const MediaAPIUsageJavaScriptFeature&) = delete;

 private:
  friend class base::NoDestructor<MediaAPIUsageJavaScriptFeature>;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_MEDIA_API_USAGE_JAVA_SCRIPT_FEATURE_H_
