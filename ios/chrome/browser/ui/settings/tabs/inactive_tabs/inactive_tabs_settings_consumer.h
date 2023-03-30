// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_CONSUMER_H_

// The consumer protocol for the inactive tabs settings.
@protocol InactiveTabsSettingsConsumer

// Called to update the current state of inactive tabs.
- (void)updateCheckedStateWithDaysThreshold:(int)threshold;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_CONSUMER_H_
