// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_IOS_CHROME_CROSS_DEVICE_PREF_PROVIDER_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_IOS_CHROME_CROSS_DEVICE_PREF_PROVIDER_H_

#import "base/containers/flat_set.h"
#import "components/sync_preferences/cross_device_pref_tracker/common_cross_device_pref_provider.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_provider.h"

// iOS implementation of `CrossDevicePrefProvider`.
class IOSChromeCrossDevicePrefProvider
    : public sync_preferences::CrossDevicePrefProvider {
 public:
  IOSChromeCrossDevicePrefProvider();
  ~IOSChromeCrossDevicePrefProvider() override;

  // `CrossDevicePrefProvider` overrides:
  const base::flat_set<std::string_view>& GetProfilePrefs() const override;
  const base::flat_set<std::string_view>& GetLocalStatePrefs() const override;

 private:
  // This defines the list of prefs that are tracked across all platforms.
  sync_preferences::CommonCrossDevicePrefProvider
      common_cross_device_pref_provider_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_IOS_CHROME_CROSS_DEVICE_PREF_PROVIDER_H_
