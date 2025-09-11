// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/ios_chrome_cross_device_pref_provider.h"

#import <string_view>

#import "base/no_destructor.h"

namespace {

// Helper to return a common, static empty set.
const base::flat_set<std::string_view>& GetEmptySet() {
  static const base::NoDestructor<base::flat_set<std::string_view>> kEmptySet;
  return *kEmptySet;
}

// Helper function to combine common prefs with platform-specific prefs.
base::flat_set<std::string_view> CombinePrefs(
    const base::flat_set<std::string_view>& common_prefs,
    const base::flat_set<std::string_view>& platform_prefs) {
  base::flat_set<std::string_view> combined_prefs = platform_prefs;
  combined_prefs.insert(common_prefs.begin(), common_prefs.end());
  return combined_prefs;
}

}  // namespace

IOSChromeCrossDevicePrefProvider::IOSChromeCrossDevicePrefProvider()
    : profile_prefs_(
          CombinePrefs(common_cross_device_pref_provider_.GetProfilePrefs(),
                       GetEmptySet())),
      local_state_prefs_(
          CombinePrefs(common_cross_device_pref_provider_.GetLocalStatePrefs(),
                       GetEmptySet())) {}

IOSChromeCrossDevicePrefProvider::~IOSChromeCrossDevicePrefProvider() = default;

const base::flat_set<std::string_view>&
IOSChromeCrossDevicePrefProvider::GetProfilePrefs() const {
  return profile_prefs_;
}

const base::flat_set<std::string_view>&
IOSChromeCrossDevicePrefProvider::GetLocalStatePrefs() const {
  return local_state_prefs_;
}
