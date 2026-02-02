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

bool IsLensOverlayAllowedByPolicy(const PrefService* prefs) {
  CHECK(prefs);
  int policyRawValue = prefs->GetInteger(lens::prefs::kLensOverlaySettings);
  return policyRawValue ==
         static_cast<int>(
             lens::prefs::LensOverlaySettingsPolicyValue::kEnabled);
}

bool IsLensOverlaySameTabNavigationEnabled(const PrefService* prefs) {
  bool isIPhone = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  return isIPhone;
}

bool IsLensOverlayLandscapeOrientationEnabled(const PrefService* prefs) {
  return base::FeatureList::IsEnabled(kLensOverlayEnableLandscapeCompatibility);
}

bool IsLVFEscapeHatchEnabled(const PrefService* prefs) {
  BOOL isTablet = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  return !isTablet;
}

bool UseCustomLensOverlayBottomSheet() {
  return base::FeatureList::IsEnabled(kLensOverlayCustomBottomSheet);
}
