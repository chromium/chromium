// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_HEADER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_HEADER_H_

#import <UIKit/UIKit.h>

@class InactiveTabsButton;

// Displayed at the top of the Tab Grid when there are inactive tabs.
@interface InactiveTabsButtonHeader : UICollectionReusableView

// The embedded button.
@property(nonatomic, strong) InactiveTabsButton* button;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_HEADER_H_
