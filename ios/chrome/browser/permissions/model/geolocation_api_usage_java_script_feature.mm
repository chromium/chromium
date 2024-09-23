// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/geolocation_api_usage_java_script_feature.h"

#import <CoreLocation/CoreLocation.h>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/geolocation/model/geolocation_manager.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "GeolocationAPI" in src/tools/metrics/histograms/enums.xml.
enum class GeolocationAPI {
  kClearWatch = 0,
  kGetCurrentPosition = 1,
  kWatchPosition = 2,
  kMaxValue = kWatchPosition,
};

const char kScriptName[] = "geolocation_overrides";

const char kGeolocationAPIAccessedHistogramAuthorized[] =
    "IOS.JavaScript.Permissions.Geolocation.Authorized";
const char kGeolocationAPIAccessedHistogramDenied[] =
    "IOS.JavaScript.Permissions.Geolocation.Denied";
const char kGeolocationAPIAccessedHistogramNotDetermined[] =
    "IOS.JavaScript.Permissions.Geolocation.NotDetermined";
const char kGeolocationAPIAccessedHistogramRestricted[] =
    "IOS.JavaScript.Permissions.Geolocation.Restricted";

const char kGeolocationAPIAccessedHandlerName[] =
    "GeolocationAPIAccessedHandler";

static const char kScriptMessageResponseAPINameKey[] = "api";

static const char kScriptMessageResponseAPIClearWatch[] = "clearWatch";
static const char kScriptMessageResponseAPIGetCurrentPosition[] =
    "getCurrentPosition";
static const char kScriptMessageResponseAPIWatchPosition[] = "watchPosition";

}  // namespace

// static
GeolocationAPIUsageJavaScriptFeature*
GeolocationAPIUsageJavaScriptFeature::GetInstance() {
  static base::NoDestructor<GeolocationAPIUsageJavaScriptFeature> instance;
  return instance.get();
}

// static
bool GeolocationAPIUsageJavaScriptFeature::ShouldOverrideAPI() {
  CLAuthorizationStatus status =
      [GeolocationManager sharedInstance].authorizationStatus;

  // A switch statement without a default case ensures that each enum value is
  // correctly handled, and future values will be handled correctly.
  switch (status) {
    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      return false;
    case kCLAuthorizationStatusNotDetermined:
    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
      return true;
  }
}

GeolocationAPIUsageJavaScriptFeature::GeolocationAPIUsageJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kPageContentWorld,
                        {FeatureScript::CreateWithFilename(
                            kScriptName,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames,
                            FeatureScript::ReinjectionBehavior::
                                kReinjectOnDocumentRecreation)}) {}
GeolocationAPIUsageJavaScriptFeature::~GeolocationAPIUsageJavaScriptFeature() =
    default;

std::optional<std::string>
GeolocationAPIUsageJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kGeolocationAPIAccessedHandlerName;
}

void GeolocationAPIUsageJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& script_message) {
  const base::Value::Dict* script_dict =
      script_message.body() ? script_message.body()->GetIfDict() : nullptr;
  if (!script_dict) {
    return;
  }

  const std::string* api =
      script_dict->FindString(kScriptMessageResponseAPINameKey);
  if (!api) {
    return;
  }

  std::string metric_name;
  switch ([GeolocationManager sharedInstance].authorizationStatus) {
    case kCLAuthorizationStatusNotDetermined:
      metric_name = kGeolocationAPIAccessedHistogramNotDetermined;
      break;
    case kCLAuthorizationStatusRestricted:
      metric_name = kGeolocationAPIAccessedHistogramRestricted;
      break;
    case kCLAuthorizationStatusDenied:
      metric_name = kGeolocationAPIAccessedHistogramDenied;
      break;
    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      metric_name = kGeolocationAPIAccessedHistogramAuthorized;
      break;
  }

  if (*api == kScriptMessageResponseAPIClearWatch) {
    base::UmaHistogramEnumeration(metric_name, GeolocationAPI::kClearWatch);
  } else if (*api == kScriptMessageResponseAPIGetCurrentPosition) {
    base::UmaHistogramEnumeration(metric_name,
                                  GeolocationAPI::kGetCurrentPosition);
  } else if (*api == kScriptMessageResponseAPIWatchPosition) {
    base::UmaHistogramEnumeration(metric_name, GeolocationAPI::kWatchPosition);
  }
}
