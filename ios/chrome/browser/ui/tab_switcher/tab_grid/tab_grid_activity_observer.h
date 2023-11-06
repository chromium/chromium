// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_ACTIVITY_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_ACTIVITY_OBSERVER_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// This delegate is used to notify any tab grid activity that might be relevant.
@protocol TabGridActivityObserver

// Notifies the receiver the page which contain the last active tab.
- (void)updateLastActiveTabPage:(TabGridPage)page;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_ACTIVITY_OBSERVER_H_
