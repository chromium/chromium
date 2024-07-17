// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_FAVICON_GRID_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_FAVICON_GRID_H_

#import <UIKit/UIKit.h>

// A 2×2 grid of favicons to appear in a TabGroupsPanelCell.
@interface TabGroupsPanelFaviconGrid : UIView

// The total number of tabs in the represented group. If there are more than 4
// tabs, an overflow counter is displayed in the bottom-trailing position.
@property(nonatomic, assign) NSUInteger numberOfTabs;

// The favicons to display. Up to the first 4 favicons are used, based on
// `numberOfTabs`.
@property(nonatomic, copy) NSArray<UIImage*>* favicons;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_FAVICON_GRID_H_
