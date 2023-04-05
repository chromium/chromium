// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_INFO_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_INFO_CONSUMER_H_

#import <Foundation/Foundation.h>

// Gets information about the current inactive tabs.
@protocol InactiveTabsInfoConsumer

// Tells the consumer the number of currently inactive tabs.
- (void)updateInactiveTabsCount:(NSInteger)count;

// Tells the consumer the number of days after which a tab is considered
// inactive.
- (void)updateInactiveTabsDaysThreshold:(NSInteger)daysThreshold;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_INFO_CONSUMER_H_
