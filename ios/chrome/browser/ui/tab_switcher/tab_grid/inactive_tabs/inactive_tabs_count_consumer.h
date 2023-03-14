// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COUNT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COUNT_CONSUMER_H_

#import <Foundation/Foundation.h>

@protocol InactiveTabsCountConsumer

// Tells the consumer to display a call-to-action regarding the current inactive
// tabs.
- (void)advertizeInactiveTabsWithCount:(NSUInteger)count;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_COUNT_CONSUMER_H_
