// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_ACTION_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_ACTION_WRANGLER_H_

#import <Foundation/Foundation.h>

// Action handler for the tab grid toolbars. This will be removed in a future
// CL. Each method is the result of an action on a toolbar button.
// TODO(crbug.com/1456659): Remove this class.
@protocol TabGridToolbarsActionWrangler

- (void)doneButtonTapped:(id)sender;

- (void)selectAllButtonTapped:(id)sender;

- (void)searchButtonTapped:(id)sender;

- (void)cancelSearchButtonTapped:(id)sender;

- (void)closeSelectedTabs:(id)sender;

- (void)shareSelectedTabs:(id)sender;

- (void)pageControlChangedValue:(id)sender;

- (void)pageControlChangedPageByDrag:(id)sender;

- (void)pageControlChangedPageByTap:(id)sender;

- (void)selectTabsButtonTapped:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_ACTION_WRANGLER_H_
