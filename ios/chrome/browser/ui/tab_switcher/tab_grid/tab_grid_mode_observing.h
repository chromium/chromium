// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MODE_OBSERVING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MODE_OBSERVING_H_

#import <UIKit/UIKit.h>

@class TabGridModeHolder;

// Observer protocol for the TabGridModeHolder.
@protocol TabGridModeObserving

// Called when the mode of the TabGrid has changed.
- (void)tabGridModeDidChange:(TabGridModeHolder*)modeHolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MODE_OBSERVING_H_
