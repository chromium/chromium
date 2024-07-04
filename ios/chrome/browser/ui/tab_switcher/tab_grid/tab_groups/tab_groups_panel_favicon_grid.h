// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_FAVICON_GRID_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_FAVICON_GRID_H_

#import <UIKit/UIKit.h>

// A 2×2 grid of favicons to appear in a TabGroupsPanelCell.
@interface TabGroupsPanelFaviconGrid : UIView

// The favicons to display. If there are more than 4 favicons, the first three
// favicons are displayed, and the bottom-trailing favicon is replaced with the
// count of overflowing favicons.
@property(nonatomic, copy) NSArray<UIImage*>* favicons;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_FAVICON_GRID_H_
