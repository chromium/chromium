// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_DARK_MODE_DETECTION_DARK_MODE_DETECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_DARK_MODE_DETECTION_DARK_MODE_DETECTION_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A feature that detects if a webpage natively supports dark mode.
class DarkModeDetectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static DarkModeDetectionJavaScriptFeature* GetInstance();

  DarkModeDetectionJavaScriptFeature(
      const DarkModeDetectionJavaScriptFeature&) = delete;
  DarkModeDetectionJavaScriptFeature& operator=(
      const DarkModeDetectionJavaScriptFeature&) = delete;

 private:
  friend class base::NoDestructor<DarkModeDetectionJavaScriptFeature>;

  DarkModeDetectionJavaScriptFeature();
  ~DarkModeDetectionJavaScriptFeature() override;

  // web::JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_DARK_MODE_DETECTION_DARK_MODE_DETECTION_JAVA_SCRIPT_FEATURE_H_
