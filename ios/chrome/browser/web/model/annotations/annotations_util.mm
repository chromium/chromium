// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/annotations/annotations_util.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/web/model/annotations/annotations_util.h"
#import "ios/web/common/features.h"

namespace {

// The policy property for default value.
const char kDefaultType[] = "default";

// Convenience converter from type to the policy property string.
std::string KeyForType(WebAnnotationType type) {
  switch (type) {
    case WebAnnotationType::kAddresses:
      return "addresses";
    case WebAnnotationType::kCalendar:
      return "calendar";
    case WebAnnotationType::kEMailAddresses:
      return "email";
    case WebAnnotationType::kPackage:
      return "package";
    case WebAnnotationType::kPhoneNumbers:
      return "phonenumbers";
    case WebAnnotationType::kUnits:
      return "units";
  }
}

// Convenience converter from policy value to the WebAnnotationPolicyValue.
std::optional<WebAnnotationPolicyValue> ValueForString(
    const std::string& string) {
  if (string == "disabled") {
    return WebAnnotationPolicyValue::kDisabled;
  }
  if (string == "longpressonly") {
    return WebAnnotationPolicyValue::kLongPressOnly;
  }
  if (string == "enabled") {
    return WebAnnotationPolicyValue::kEnabled;
  }
  return std::nullopt;
}
}  // namespace

WebAnnotationPolicyValue GetPolicyForType(PrefService* prefs,
                                          WebAnnotationType type) {
  const auto& prefs_dict = prefs->GetDict(prefs::kWebAnnotationsPolicy);
  const std::string* value_string = prefs_dict.FindString(KeyForType(type));
  if (value_string) {
    auto value = ValueForString(*value_string);
    if (value) {
      return *value;
    }
  }
  value_string = prefs_dict.FindString(kDefaultType);
  if (value_string) {
    auto value = ValueForString(*value_string);
    if (value) {
      return *value;
    }
  }
  return WebAnnotationPolicyValue::kEnabled;
}

bool IsAddressDetectionEnabled() {
  if (@available(iOS 16.4, *)) {
    return base::FeatureList::IsEnabled(web::features::kOneTapForMaps);
  }
  return false;
}

bool IsAddressAutomaticDetectionEnabled(PrefService* prefs) {
  return IsAddressDetectionEnabled() &&
         prefs->GetBoolean(prefs::kDetectAddressesEnabled);
}

bool IsAddressAutomaticDetectionAccepted(PrefService* prefs) {
  return IsAddressDetectionEnabled() &&
         prefs->GetBoolean(prefs::kDetectAddressesAccepted);
}

bool ShouldPresentConsentIPH(PrefService* prefs) {
  std::string param = base::GetFieldTrialParamValueByFeature(
      web::features::kOneTapForMaps,
      web::features::kOneTapForMapsConsentModeParamTitle);
  if (param == web::features::kOneTapForMapsConsentModeIPHForcedParam) {
    return true;
  }
  if (param == web::features::kOneTapForMapsConsentModeIPHParam) {
    return !IsAddressAutomaticDetectionAccepted(prefs);
  }
  return false;
}

bool ShouldPresentConsentScreen(PrefService* prefs) {
  std::string param = base::GetFieldTrialParamValueByFeature(
      web::features::kOneTapForMaps,
      web::features::kOneTapForMapsConsentModeParamTitle);
  if (param == web::features::kOneTapForMapsConsentModeForcedParam) {
    return true;
  }
  if (param == web::features::kOneTapForMapsConsentModeDisabledParam ||
      param == web::features::kOneTapForMapsConsentModeIPHParam ||
      param == web::features::kOneTapForMapsConsentModeIPHForcedParam) {
    return false;
  }
  return !IsAddressAutomaticDetectionAccepted(prefs);
}

bool IsAddressLongPressDetectionEnabled(PrefService* prefs) {
  return !IsAddressDetectionEnabled() ||
         prefs->GetBoolean(prefs::kDetectAddressesEnabled);
}

bool IsUnitAutomaticDetectionEnabled(PrefService* prefs) {
  return (base::FeatureList::IsEnabled(web::features::kEnableMeasurements) &&
          prefs->GetBoolean(prefs::kDetectUnitsEnabled));
}

bool IsLongPressAnnotationEnabledForType(PrefService* prefs,
                                         WebAnnotationType type) {
  switch (GetPolicyForType(prefs, type)) {
    case WebAnnotationPolicyValue::kDisabled:
      return false;
    case WebAnnotationPolicyValue::kEnabled:
    case WebAnnotationPolicyValue::kLongPressOnly:
      break;
  }
  switch (type) {
    case WebAnnotationType::kAddresses:
      return IsAddressLongPressDetectionEnabled(prefs);
    case WebAnnotationType::kCalendar:
      return true;
    case WebAnnotationType::kEMailAddresses:
      return true;
    case WebAnnotationType::kPackage:
      return IsIOSParcelTrackingEnabled();
    case WebAnnotationType::kPhoneNumbers:
      return true;
    case WebAnnotationType::kUnits:
      return IsUnitAutomaticDetectionEnabled(prefs);
  }
}

bool IsOneTapAnnotationEnabledForType(PrefService* prefs,
                                      WebAnnotationType type) {
  switch (GetPolicyForType(prefs, type)) {
    case WebAnnotationPolicyValue::kDisabled:
    case WebAnnotationPolicyValue::kLongPressOnly:
      return false;
    case WebAnnotationPolicyValue::kEnabled:
      break;
  }
  switch (type) {
    case WebAnnotationType::kAddresses:
      return IsAddressAutomaticDetectionEnabled(prefs);
    case WebAnnotationType::kCalendar:
      return true;
    case WebAnnotationType::kEMailAddresses:
      return true;
    case WebAnnotationType::kPackage:
      return IsIOSParcelTrackingEnabled();
    case WebAnnotationType::kPhoneNumbers:
      return true;
    case WebAnnotationType::kUnits:
      return IsUnitAutomaticDetectionEnabled(prefs);
  }
}
