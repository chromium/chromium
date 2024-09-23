// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSUMER_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@protocol GridConsumer

// Sets the active page.
- (void)setActivePageFromPage:(TabGridPage)page;
// Notify consumer that the grid will be dismissed.
- (void)prepareForDismissal;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSUMER_H_
