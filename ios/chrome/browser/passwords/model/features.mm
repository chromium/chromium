// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/features.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

BASE_FEATURE(kImportPasswordsFromSafari, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordManagerEnableCrowdsourcingUploads,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldShowSafariImportWorkflow(ProfileIOS* profile) {
  if (!profile) {
    return false;
  }

  // For the time being, managed users are not eligible for Safari Import.
  ProfilePolicyConnector* connector = profile->GetPolicyConnector();
  if (connector && connector->IsManaged()) {
    return false;
  }

  // Safari export is not available on iOS versions earlier than 18.2.
  if (@available(iOS 18.2, *)) {
    return base::FeatureList::IsEnabled(kImportPasswordsFromSafari);
  }
  return false;
}
