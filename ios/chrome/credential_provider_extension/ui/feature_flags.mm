// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_field_trial_version.h"
#import "ios/chrome/common/credential_provider/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BOOL IsPasswordCreationUserEnabled() {
  return [[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()]
      boolValue];
}

BOOL IsPasswordManagerBrandingUpdateEnable() {
  NSDictionary* allFeatures = [app_group::GetGroupUserDefaults()
      objectForKey:app_group::kChromeExtensionFieldTrialPreference];
  NSDictionary* featureData =
      allFeatures[@"IOSEnablePasswordManagerBrandingUpdate"];
  if (!featureData || kPasswordManagerBrandingUpdateFeatureVersion !=
                          [featureData[kFieldTrialVersionKey] intValue]) {
    return NO;
  }
  return [featureData[kFieldTrialValueKey] boolValue];
}
