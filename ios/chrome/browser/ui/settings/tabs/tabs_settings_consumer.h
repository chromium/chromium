// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_CONSUMER_H_

// The consumer protocol for the tabs settings.
@protocol TabsSettingsConsumer

// Called when the value of prefs::kInactiveTabsTimeThreshold changed.
- (void)setInactiveTabsTimeThreshold:(int)threshold;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_CONSUMER_H_
