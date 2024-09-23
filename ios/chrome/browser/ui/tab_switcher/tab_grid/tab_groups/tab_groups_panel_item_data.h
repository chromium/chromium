// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_H_

#import <UIKit/UIKit.h>

// Holds properties to configure a Tab Groups panel cell. These are the
// properties that are synchronously retrievable. Favicons must be retrieved
// asynchronously and are not part of this data holder.
@interface TabGroupsPanelItemData : NSObject

// The title of the tab group.
@property(nonatomic, copy) NSString* title;

// The color of the tab group.
@property(nonatomic, strong) UIColor* color;

// The localized creation text of the tab group.
@property(nonatomic, copy) NSString* creationText;

// The number of tabs in the tab group.
@property(nonatomic, assign) NSUInteger numberOfTabs;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_H_
