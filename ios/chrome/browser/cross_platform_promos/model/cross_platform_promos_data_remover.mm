// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_data_remover.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

CrossPlatformPromosDataRemover::CrossPlatformPromosDataRemover(
    ProfileIOS* profile)
    : profile_(profile) {
  CHECK(profile_);
}

void CrossPlatformPromosDataRemover::Remove() {
  profile_->GetPrefs()->ClearPref(prefs::kCrossPlatformPromosActiveDays);
  profile_->GetPrefs()->ClearPref(prefs::kCrossPlatformPromosIOS16thActiveDay);
}
