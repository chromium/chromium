// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_H_

#import <UIKit/UIKit.h>

// The button to advertize Inactive Tabs. It displays the count of inactive
// tabs.
@interface InactiveTabsButton : UIButton

// The number of Inactive Tabs to advertize. If set to something above 100, the
// label will display "99+".
@property(nonatomic, assign) NSUInteger count;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_H_
