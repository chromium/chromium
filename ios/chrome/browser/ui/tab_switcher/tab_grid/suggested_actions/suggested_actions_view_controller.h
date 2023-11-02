// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol SuggestedActionsDelegate;
@class SuggestedActionsViewController;

// Protocol used to relay relevant user interactions from the view.
@protocol SuggestedActionsViewControllerDelegate
// Tells the delegate that the user tapped on the search in history item.
- (void)didSelectSearchHistoryInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController;
// Tells the delegate that the user tapped on the search in reecent tabs item.
- (void)didSelectSearchRecentTabsInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController;
// Tells the delegate that the user tapped on search in web item.
- (void)didSelectSearchWebInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController;
// Asks the delegate to fetch the history results count and execute `completion`
// with it.
- (void)suggestedActionsViewController:
            (SuggestedActionsViewController*)viewController
    fetchHistoryResultsCountWithCompletion:(void (^)(size_t))completion;

@end

// SuggestedActionsViewController represents the suggestions will appear on
// the tab grid to extend the users search journey beyond the current active
// page of the tab grid.
@interface SuggestedActionsViewController : LegacyChromeTableViewController

- (instancetype)initWithDelegate:
    (id<SuggestedActionsViewControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// The delegate to handle interactions with view.
@property(nonatomic, weak) id<SuggestedActionsViewControllerDelegate> delegate;
// The current search term associated that the suggestion actions are being
// presented for.
@property(nonatomic, copy) NSString* searchText;
@property(nonatomic, readonly) CGFloat contentHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_VIEW_CONTROLLER_H_
