// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_policy_utils.h"

#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

bool IsScreenshotsDisabledByPolicy(const ProfileIOS* profile) {
  return profile && IsDisableScreenshotsPolicyIOSEnabled() &&
         profile->GetPrefs()->GetBoolean(
             policy::policy_prefs::kDisableScreenshots);
}
