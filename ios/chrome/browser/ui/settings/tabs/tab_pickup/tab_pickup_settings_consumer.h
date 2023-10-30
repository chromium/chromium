// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_CONSUMER_H_

// Tab-sync states.
enum TabSyncState {
  // Enabled.
  kEnabled,
  // Disabled or signed out.
  kDisabled,
  // Disabled by an Enterprise policy.
  kDisabledByPolicy,
  // Sign-in disabled by the user.
  kDisabledByUser,
};

// The consumer protocol for the tab pickup settings.
@protocol TabPickupSettingsConsumer

// Called when the value of prefs::kTabPickupEnabled changed.
- (void)setTabPickupEnabled:(BOOL)enabled;

// Called when the tab-sync state changed.
- (void)setTabSyncState:(TabSyncState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_CONSUMER_H_
