// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// UIViewController used to display pinned tabs.
@interface PinnedTabsViewController : UIViewController

// Makes the pinned tabs view available. The pinned view should only be
// available when the regular tabs grid is displayed.
- (void)pinnedTabsAvailable:(BOOL)available;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_
