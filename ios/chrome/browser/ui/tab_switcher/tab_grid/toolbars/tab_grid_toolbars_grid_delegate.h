// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_GRID_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_GRID_DELEGATE_H_

// Delegate for the tab grid toolbars buttons acting on the grid. Each method is
// the result of an action on a toolbar button.
@protocol TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender;
- (void)doneButtonTapped:(id)sender;
- (void)newTabButtonTapped:(id)sender;
- (void)selectAllButtonTapped:(id)sender;
- (void)searchButtonTapped:(id)sender;
- (void)cancelSearchButtonTapped:(id)sender;
- (void)closeSelectedTabs:(id)sender;
- (void)shareSelectedTabs:(id)sender;
- (void)selectTabsButtonTapped:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_GRID_DELEGATE_H_
