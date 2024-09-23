// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_CONFIGURATION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Tab grid toolbars configuration used by bottom and top tab grid toolbars.
@interface TabGridToolbarsConfiguration : NSObject

// The page for which this configuration is created.
@property(nonatomic, assign, readonly) TabGridPage page;

// NORMAL MODE ====================
// YES if the button should be displayed.
// NOTE: This is different from being enabled. These buttons can be displayed
// but not enabled (grayed out).
@property(nonatomic) BOOL closeAllButton;
@property(nonatomic) BOOL selectTabsButton;
@property(nonatomic) BOOL undoButton;

// YES if the button should be enabled. If NO, the button is grayed out.
@property(nonatomic) BOOL doneButton;
@property(nonatomic) BOOL newTabButton;

// SELECTION MODE =================
// YES if displayed, specific to tab selection mode.
@property(nonatomic) BOOL deselectAllButton;
@property(nonatomic) BOOL selectAllButton;

// YES if enabled, specific to tab selection mode.
@property(nonatomic) BOOL addToButton;
@property(nonatomic) BOOL closeSelectedTabsButton;
@property(nonatomic) BOOL shareButton;

@property(nonatomic) UIMenu* addToButtonMenu;
@property(nonatomic) NSUInteger selectedItemsCount;

// SEARCH MODE =====================
// The value of this buttons, do not have any impact yet.
// TODO(crbug.com/40273478): Move the following button when their configuration
// is taken into account.
@property(nonatomic) BOOL cancelSearchButton;
@property(nonatomic) BOOL searchButton;

// Returns a configuration disabling all buttons.
+ (TabGridToolbarsConfiguration*)disabledConfigurationForPage:(TabGridPage)page;

- (instancetype)initWithPage:(TabGridPage)page;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_CONFIGURATION_H_
