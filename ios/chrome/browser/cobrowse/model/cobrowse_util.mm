// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_util.h"

#import "components/omnibox/browser/aim_eligibility_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

bool IsAimCobrowseEligible(ProfileIOS* profile) {
  if (!IsAimCobrowseEnabled() || !IsAssistantContainerEnabled()) {
    return false;
  }

  if (experimental_flags::ShouldForceDisableComposeboxAIM()) {
    return false;
  }

  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile);
  if (!aim_eligibility_service ||
      !aim_eligibility_service->IsFuseboxEligible() ||
      !aim_eligibility_service->IsCobrowseEligible()) {
    return false;
  }

  return true;
}
