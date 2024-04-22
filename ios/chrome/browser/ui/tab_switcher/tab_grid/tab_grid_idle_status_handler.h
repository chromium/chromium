// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_IDLE_STATUS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_IDLE_STATUS_HANDLER_H_

/// The types of action the user can perform on the tab grid.
enum class TabGridActionType {
  /// In page actions, including the following:
  /// - Selecting another tab or tab group
  /// - Creating, closing, moving, long pressing, grouping and ungrouping tabs
  /// - Entering search or selection mode
  /// - Entering inactive tabs
  kInPageAction,
  /// Actions of switching between pages.
  kChangePage,
  /// Action of putting app to background
  kBackground,
};

/// Handler protocol that tracks the idle status of the track grid.
@protocol TabGridIdleStatusHandler

/// Idle status taking into account of all possible `TabGridActionType`s. If
/// `YES`, the user has not done anything meaningful since entering the tab
/// grid.
- (BOOL)status;

/// Informs the handler that  tab grid action of `type` has happened.
- (void)tabGridDidPerformAction:(TabGridActionType)type;

@end
#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_IDLE_STATUS_HANDLER_H_
