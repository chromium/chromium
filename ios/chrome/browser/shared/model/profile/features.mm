// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/features.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool AreSeparateProfilesForManagedAccountsEnabled() {
  // The APIs to support multiple profiles are only available in iOS 17+, so
  // consider this feature as disabled in earlier versions.
  if (!@available(iOS 17, *)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kSeparateProfilesForManagedAccounts);
}

bool IsIdentityDiscAccountMenuEnabled() {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kIdentityDiscAccountMenu);
}
