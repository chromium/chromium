// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTEXT_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTEXT_MENU_PROVIDER_H_

#import <UIKit/UIKit.h>

@class GridCell;

// Protocol for instances that will provide menus to the Grid view.
@protocol GridContextMenuProvider

// Returns a context menu configuration instance for the given |cell| and its
// associated |view|.
- (UIContextMenuConfiguration*)contextMenuConfigurationForCell:(GridCell*)cell
                                                      fromView:(UIView*)view
    API_AVAILABLE(ios(13.0));

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONTEXT_MENU_PROVIDER_H_
