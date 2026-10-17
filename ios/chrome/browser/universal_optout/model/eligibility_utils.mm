// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/universal_optout/model/eligibility_utils.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/prefs.h"
#import "components/universal_optout/universal_optout_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/universal_optout/model/universal_optout_service_factory.h"
#import "ios/web/public/web_client.h"

namespace universal_optout {

bool IsGPCExtensionSupportedOS() {
  if (@available(iOS 18.4, *)) {
    return !base::ios::IsRunningOnIOS27OrLater();
  }
  return false;
}

web::UniversalOptOutState GetUniversalOptOutState(
    PrefService* prefs,
    UniversalOptOutService* optout_service) {
  // If explicitly forced off via experimental flag, return not eligible
  // regardless of preference or service state.
  if (experimental_flags::GetUniversalOptOutEligibilityOverride() ==
      experimental_flags::UniversalOptOutEligibilityOverride::kForcedOff) {
    return web::UniversalOptOutState::kNotEligible;
  }

  // Preconditions: evaluate eligibility and enabled state first before checking
  // feature flags to prevent premature Finch experiment assignment.
  bool is_eligible = false;
  switch (experimental_flags::GetUniversalOptOutEligibilityOverride()) {
    case experimental_flags::UniversalOptOutEligibilityOverride::kForcedOn:
      is_eligible = true;
      break;
    case experimental_flags::UniversalOptOutEligibilityOverride::kDefault:
      if (optout_service) {
        is_eligible = optout_service->IsEligible();
      } else if (prefs) {
        is_eligible = prefs->GetBoolean(prefs::kUniversalOptOutEligible);
      }
      break;
    case experimental_flags::UniversalOptOutEligibilityOverride::kForcedOff:
      NOTREACHED();
  }

  const bool is_enabled =
      prefs && prefs->GetBoolean(prefs::kUniversalOptOutEnabled);

  if (!is_eligible && !is_enabled) {
    return web::UniversalOptOutState::kNotEligible;
  }

  // Check feature flags after verifying preconditions.
  if (!features::IsUniversalOptOutEnabled() ||
      !base::FeatureList::IsEnabled(features::kUniversalOptOutSettings)) {
    return web::UniversalOptOutState::kNotEligible;
  }

  if (is_enabled) {
    return web::UniversalOptOutState::kEnabled;
  }

  return web::UniversalOptOutState::kEligible;
}

bool IsGPCExtensionEnabled(ProfileIOS* profile) {
  if (!profile) {
    return false;
  }
  if (!IsGPCExtensionSupportedOS()) {
    return false;
  }
  UniversalOptOutService* optout_service =
      UniversalOptOutServiceFactory::GetForProfile(profile);
  if (!optout_service) {
    return false;
  }
  if (!features::IsUniversalOptOutEnabled() ||
      !base::FeatureList::IsEnabled(features::kUniversalOptOutSettings)) {
    return false;
  }
  if (features::IsUniversalOptOutExtensionSilentEnabled()) {
    return false;
  }
  return true;
}

}  // namespace universal_optout
