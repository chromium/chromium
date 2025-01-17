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
bool IsLensOverlayAllowedByPolicy() {
  int policyRawValue = GetApplicationContext()->GetLocalState()->GetInteger(
      lens::prefs::kLensOverlaySettings);
  return policyRawValue ==
         static_cast<int>(
             lens::prefs::LensOverlaySettingsPolicyValue::kEnabled);
}

// Returns whether the lens overlay is enabled.
bool IsLensOverlayAvailable() {
  bool featureEnabled = base::FeatureList::IsEnabled(kEnableLensOverlay);
  bool forceIPadEnabled =
      base::FeatureList::IsEnabled(kLensOverlayEnableIPadCompatibility);
  bool isIPhone = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  return featureEnabled && (forceIPadEnabled || isIPhone) &&
         IsLensOverlayAllowedByPolicy();
}

bool IsLensOverlaySameTabNavigationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayEnableSameTabNavigation);
}

bool IsLVFUnifiedExperienceEnabled() {
  return base::FeatureList::IsEnabled(kEnableLensViewFinderUnifiedExperience);
}

LensOverlayOnboardingTreatment GetLensOverlayOnboardingTreatment() {
  LensOverlayOnboardingTreatment fallbackDefault =
      LensOverlayOnboardingTreatment::kDefaultOnboardingExperience;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool hasOnboardingSwitch =
      command_line->HasSwitch(kLensOverlayAlternativeOnboardingType);
  if (!hasOnboardingSwitch) {
    return fallbackDefault;
  }

  std::string optionString =
      command_line->GetSwitchValueASCII(kLensOverlayAlternativeOnboardingType);

  unsigned int intValue;
  bool conversionSuccess = base::StringToUint(optionString, &intValue);
  unsigned int maxValue =
      static_cast<int>(LensOverlayOnboardingTreatment::kMaxValue);
  if (intValue > maxValue || !conversionSuccess) {
    return fallbackDefault;
  }

  return static_cast<LensOverlayOnboardingTreatment>(intValue);
}
