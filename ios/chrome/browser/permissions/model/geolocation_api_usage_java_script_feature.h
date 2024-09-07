// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_GEOLOCATION_API_USAGE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_GEOLOCATION_API_USAGE_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

// A feature which listens for usage of geolocation APIs.
class GeolocationAPIUsageJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static GeolocationAPIUsageJavaScriptFeature* GetInstance();

  static bool ShouldOverrideAPI();

  GeolocationAPIUsageJavaScriptFeature();
  ~GeolocationAPIUsageJavaScriptFeature() override;

  GeolocationAPIUsageJavaScriptFeature(
      const GeolocationAPIUsageJavaScriptFeature&) = delete;
  GeolocationAPIUsageJavaScriptFeature& operator=(
      const GeolocationAPIUsageJavaScriptFeature&) = delete;

 private:
  friend class base::NoDestructor<GeolocationAPIUsageJavaScriptFeature>;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_GEOLOCATION_API_USAGE_JAVA_SCRIPT_FEATURE_H_
