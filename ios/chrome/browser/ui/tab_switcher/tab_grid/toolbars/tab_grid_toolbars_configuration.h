// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_CONFIGURATION_H_

#import <Foundation/Foundation.h>

// Tab grid toolbars configuration used by bottom and top tab grid toolbars.
@interface TabGridToolbarsConfiguration : NSObject

// YES if the button should be displayed.
// NOTE: This is different from being enabled. These button can be displayed but
// not enabled (grayed out).
@property(nonatomic) BOOL addToButton;
@property(nonatomic) BOOL cancelSearchButton;
@property(nonatomic) BOOL closeAllButton;
@property(nonatomic) BOOL closeSelectedTabsButton;
@property(nonatomic) BOOL deselectAllButton;
@property(nonatomic) BOOL doneButton;
@property(nonatomic) BOOL searchButton;
@property(nonatomic) BOOL selectAllButton;
@property(nonatomic) BOOL selectTabsButton;
@property(nonatomic) BOOL shareButton;
@property(nonatomic) BOOL undoButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_CONFIGURATION_H_
