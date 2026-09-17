// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace gemini {

#pragma mark - Features

namespace {

// Returns whether the specified feature is available for the given account.
bool IsFeatureAvailable(Feature feature, const AccountInfo& account_info) {
  switch (feature) {
    case Feature::kImageRemix:
      return HasModelExecutionCapability(account_info);
    case Feature::kLive:
      return IsGeminiLiveEnabled() && HasModelExecutionCapability(account_info);
  }
}

}  // namespace

bool IsFeatureAvailable(Feature feature, ProfileIOS* profile) {
  if (!profile) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return IsFeatureAvailable(feature, identity_manager);
}

bool IsFeatureAvailable(Feature feature,
                        signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return false;
  }

  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  return IsFeatureAvailable(feature, account_info);
}

#pragma mark - Capabilities

bool HasGeminiInChromeCapability(const AccountInfo& account_info) {
  if (account_info.IsEmpty()) {
    return false;
  }

  const AccountCapabilities capabilities =
      account_info.GetAccountCapabilities();

  return signin::TriboolToBoolOr(capabilities.can_use_gemini_in_chrome(),
                                 false);
}

bool HasModelExecutionCapability(const AccountInfo& account_info) {
  if (account_info.IsEmpty()) {
    return false;
  }

  const AccountCapabilities capabilities =
      account_info.GetAccountCapabilities();

  return signin::TriboolToBoolOr(
      capabilities.can_use_model_execution_features(), false);
}

}  // namespace gemini
