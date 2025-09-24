// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/aim_availability.h"

#import "components/omnibox/browser/aim_eligibility_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ui/base/device_form_factor.h"

bool IsAIMAvailable(ProfileIOS* profile) {
  CHECK(profile);

  // Only on phone.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }

  return AimEligibilityService::GenericKillSwitchFeatureCheck(
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile),
      kNTPMIAEntrypointAllLocales, kNTPMIAEntrypoint);
}
