// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_CONSUMER_H_

// The consumer protocol for the tab pickup settings.
@protocol TabPickupSettingsConsumer

// Called when the value of prefs::kTabPickupEnabled changed.
- (void)setTabPickupEnabled:(bool)enabled;

// Called when the sync feature state changed.
- (void)setSyncEnabled:(bool)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_CONSUMER_H_
