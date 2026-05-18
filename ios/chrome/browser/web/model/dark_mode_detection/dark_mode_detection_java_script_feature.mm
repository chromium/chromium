// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/dark_mode_detection/dark_mode_detection_java_script_feature.h"

#import <UIKit/UIKit.h>

#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "dark_mode_detection";
const char kScriptHandlerName[] = "DarkModeDetectionMessageHandler";

// Enum for crossing dark mode support with device dark mode state.
// LINT.IfChange(DarkModeDetectionCrossedState)
enum class DarkModeDetectionCrossedState {
  kNotSupportedLightMode = 0,
  kNotSupportedDarkMode = 1,
  kSupportedLightMode = 2,
  kSupportedDarkMode = 3,
  kMaxValue = kSupportedDarkMode
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSDarkModeDetectionCrossedState)

}  // namespace

// static
DarkModeDetectionJavaScriptFeature*
DarkModeDetectionJavaScriptFeature::GetInstance() {
  static base::NoDestructor<DarkModeDetectionJavaScriptFeature> instance;
  return instance.get();
}

DarkModeDetectionJavaScriptFeature::DarkModeDetectionJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

DarkModeDetectionJavaScriptFeature::~DarkModeDetectionJavaScriptFeature() =
    default;

std::optional<std::string>
DarkModeDetectionJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void DarkModeDetectionJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(web_state);

  GURL url = message.request_url().value_or(GURL());
  if (!url.is_valid() || url.spec() == "about:blank") {
    return;
  }

  if (!message.body() || !message.body()->is_dict()) {
    return;
  }

  const base::DictValue& dict = message.body()->GetDict();
  std::optional<bool> supports_via_meta = dict.FindBool("supportsViaMeta");
  std::optional<bool> supports_via_css = dict.FindBool("supportsViaCss");
  std::optional<bool> supports_via_media_query =
      dict.FindBool("supportsViaMediaQuery");

  if (!supports_via_meta || !supports_via_css || !supports_via_media_query) {
    return;
  }

  bool supports_dark_mode =
      *supports_via_meta || *supports_via_css || *supports_via_media_query;

  UIUserInterfaceStyle user_interface_style =
      UITraitCollection.currentTraitCollection.userInterfaceStyle;
  bool is_device_dark_mode = (user_interface_style == UIUserInterfaceStyleDark);

  auto get_crossed_state = [](bool supported, bool is_dark) {
    int bucket = 0;
    if (supported) {
      bucket += 2;
    }
    if (is_dark) {
      bucket += 1;
    }
    return static_cast<DarkModeDetectionCrossedState>(bucket);
  };

  base::UmaHistogramEnumeration(
      "IOS.DarkModeDetection.SupportsViaMeta",
      get_crossed_state(*supports_via_meta, is_device_dark_mode));
  base::UmaHistogramEnumeration(
      "IOS.DarkModeDetection.SupportsViaCss",
      get_crossed_state(*supports_via_css, is_device_dark_mode));
  base::UmaHistogramEnumeration(
      "IOS.DarkModeDetection.SupportsViaMediaQuery",
      get_crossed_state(*supports_via_media_query, is_device_dark_mode));
  base::UmaHistogramEnumeration(
      "IOS.DarkModeDetection.SupportsDarkMode",
      get_crossed_state(supports_dark_mode, is_device_dark_mode));
}
