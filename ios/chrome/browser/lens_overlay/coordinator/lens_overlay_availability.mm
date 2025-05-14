// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"

#import "base/command_line.h"
#import "base/strings/string_number_conversions.h"
#import "components/lens/lens_overlay_permission_utils.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/device_form_factor.h"

// Returns whether the lens overlay is allowed by policy.
bool IsLensOverlayAllowedByPolicy(const PrefService* prefs) {
  CHECK(prefs, kLensOverlayNotFatalUntil);
  int policyRawValue = prefs->GetInteger(lens::prefs::kLensOverlaySettings);
  return policyRawValue ==
         static_cast<int>(
             lens::prefs::LensOverlaySettingsPolicyValue::kEnabled);
}

// Returns whether the lens overlay is enabled.
bool IsLensOverlayAvailable(const PrefService* prefs) {
  bool featureEnabled = base::FeatureList::IsEnabled(kEnableLensOverlay);
  bool forceIPadEnabled =
      base::FeatureList::IsEnabled(kLensOverlayEnableIPadCompatibility);
  bool isIPhone = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  return featureEnabled && (forceIPadEnabled || isIPhone) &&
         IsLensOverlayAllowedByPolicy(prefs);
}

bool IsLensOverlaySameTabNavigationEnabled(const PrefService* prefs) {
  bool isIPhone = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  return isIPhone && IsLensOverlayAvailable(prefs) &&
         base::FeatureList::IsEnabled(kLensOverlayEnableSameTabNavigation);
}

bool IsLVFUnifiedExperienceEnabled(const PrefService* prefs) {
  return IsLensOverlayAvailable(prefs) &&
         base::FeatureList::IsEnabled(kEnableLensViewFinderUnifiedExperience);
}

bool IsLensOverlayLandscapeOrientationEnabled(const PrefService* prefs) {
  return IsLensOverlayAvailable(prefs) &&
         base::FeatureList::IsEnabled(kLensOverlayEnableLandscapeCompatibility);
}

bool IsLVFEscapeHatchEnabled(const PrefService* prefs) {
  BOOL isTablet = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  if (isTablet) {
    return NO;
  }
  return IsLensOverlayAvailable(prefs) &&
         base::FeatureList::IsEnabled(kLensOverlayEnableLVFEscapeHatch);
}

LensOverlayOnboardingTreatment GetLensOverlayOnboardingTreatment() {
  std::string featureParam = base::GetFieldTrialParamValueByFeature(
      kLensOverlayAlternativeOnboarding, kLensOverlayOnboardingParam);
  if (featureParam == kLensOverlayOnboardingParamSpeedbumpMenu) {
    return LensOverlayOnboardingTreatment::kSpeedbumpMenu;
  } else if (featureParam == kLensOverlayOnboardingParamUpdatedStrings) {
    return LensOverlayOnboardingTreatment::kUpdatedOnboardingStrings;
  } else if (featureParam ==
             kLensOverlayOnboardingParamUpdatedStringsAndVisuals) {
    return LensOverlayOnboardingTreatment::kUpdatedOnboardingStringsAndVisuals;
  } else {
    return LensOverlayOnboardingTreatment::kDefaultOnboardingExperience;
  }
}
