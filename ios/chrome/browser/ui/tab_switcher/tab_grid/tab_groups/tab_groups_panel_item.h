// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_

#import <UIKit/UIKit.h>

#include "base/uuid.h"

// Wraps the ID of a saved tab group.
@interface TabGroupsPanelItem : NSObject

// The saved group's ID.
@property(nonatomic, assign) base::Uuid savedTabGroupID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
