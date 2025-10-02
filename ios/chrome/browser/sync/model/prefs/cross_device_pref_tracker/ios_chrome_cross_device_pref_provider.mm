// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/ios_chrome_cross_device_pref_provider.h"

#import <string_view>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Helper function to combine common prefs with platform-specific prefs.
base::flat_set<std::string_view> CombinePrefs(
    const base::flat_set<std::string_view>& common_prefs,
    const base::flat_set<std::string_view>& platform_prefs) {
  base::flat_set<std::string_view> combined_prefs = platform_prefs;
  combined_prefs.insert(common_prefs.begin(), common_prefs.end());
  return combined_prefs;
}

}  // namespace

IOSChromeCrossDevicePrefProvider::IOSChromeCrossDevicePrefProvider() = default;

IOSChromeCrossDevicePrefProvider::~IOSChromeCrossDevicePrefProvider() = default;

const base::flat_set<std::string_view>&
IOSChromeCrossDevicePrefProvider::GetProfilePrefs() const {
  static const base::NoDestructor<base::flat_set<std::string_view>>
      kCombinedPrefs(
          CombinePrefs(common_cross_device_pref_provider_.GetProfilePrefs(),
                       base::flat_set<std::string_view>{
                           prefs::kCrossPlatformPromosIOS16thActiveDay,
                       }));
  return *kCombinedPrefs;
}

const base::flat_set<std::string_view>&
IOSChromeCrossDevicePrefProvider::GetLocalStatePrefs() const {
  return common_cross_device_pref_provider_.GetLocalStatePrefs();
}
