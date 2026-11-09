// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/ios_chrome_cross_device_pref_provider.h"

#import <initializer_list>
#import <string_view>
#import <utility>
#import <vector>

#import "base/containers/flat_set.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Helper function to combine common prefs with platform-specific prefs.
base::flat_set<std::string_view> CombinePrefs(
    const base::flat_set<std::string_view>& common_prefs,
    std::initializer_list<std::string_view> platform_prefs) {
  std::vector<std::string_view> combined_prefs;
  combined_prefs.reserve(common_prefs.size() + platform_prefs.size());
  combined_prefs.insert(combined_prefs.end(), common_prefs.begin(),
                        common_prefs.end());
  combined_prefs.insert(combined_prefs.end(), platform_prefs.begin(),
                        platform_prefs.end());
  return base::flat_set<std::string_view>(std::move(combined_prefs));
}

}  // namespace

IOSChromeCrossDevicePrefProvider::IOSChromeCrossDevicePrefProvider() = default;

IOSChromeCrossDevicePrefProvider::~IOSChromeCrossDevicePrefProvider() = default;

const base::flat_set<std::string_view>&
IOSChromeCrossDevicePrefProvider::GetProfilePrefs() const {
  static const base::NoDestructor<base::flat_set<std::string_view>>
      kCombinedPrefs(
          CombinePrefs(common_cross_device_pref_provider_.GetProfilePrefs(),
                       {prefs::kCrossPlatformPromosIOS16thActiveDay}));
  return *kCombinedPrefs;
}

const base::flat_set<std::string_view>&
IOSChromeCrossDevicePrefProvider::GetLocalStatePrefs() const {
  return common_cross_device_pref_provider_.GetLocalStatePrefs();
}
