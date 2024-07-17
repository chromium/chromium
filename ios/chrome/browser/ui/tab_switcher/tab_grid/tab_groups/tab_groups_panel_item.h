// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_

#import <UIKit/UIKit.h>

#include "base/uuid.h"

namespace base {
class Time;
}  // namespace base

// Wraps the ID of a saved tab group.
@interface TabGroupsPanelItem : NSObject

// The saved group's ID.
@property(nonatomic, assign) base::Uuid savedTabGroupID;

// The title of the Tab Group.
// TODO(crbug.com/350493712): Replace this with a data source method.
@property(nonatomic, copy) NSString* title;

// The color of the dot.
// TODO(crbug.com/350493712): Replace this with a data source method.
@property(nonatomic, strong) UIColor* color;

// The creation date of the Tab Group.
// TODO(crbug.com/350493712): Replace this with a data source method.
@property(nonatomic, assign) base::Time creationDate;

// The favicons of the tabs in the Tab Group.
// TODO(crbug.com/350493712): Replace this with a data source method.
@property(nonatomic, copy) NSArray<UIImage*>* favicons;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
