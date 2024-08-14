// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_MAIN_TAB_GRID_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_MAIN_TAB_GRID_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for the tab grid toolbars buttons acting on the tab grid. Each
// method is the result of an action on a toolbar button.
@protocol TabGridToolbarsMainTabGridDelegate

- (void)pageControlChangedValue:(id)sender;

- (void)pageControlChangedPageByDrag:(id)sender;

- (void)pageControlChangedPageByTap:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_MAIN_TAB_GRID_DELEGATE_H_
