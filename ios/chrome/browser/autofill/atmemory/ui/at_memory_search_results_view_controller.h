// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_RESULTS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_RESULTS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class AtMemorySearchResultItem;
@class AtMemorySearchResultsViewController;

@protocol AtMemorySearchResultsViewControllerDelegate <NSObject>
// Notifies that the info button was tapped on a search result cell.
- (void)searchResultsViewControllerDidTapInfo:
    (AtMemorySearchResultsViewController*)viewController;

// Notifies that an item was selected.
- (void)searchResultsViewController:
            (AtMemorySearchResultsViewController*)viewController
                   didSelectContent:(NSString*)content;
@end

@interface AtMemorySearchResultsViewController : ChromeTableViewController

@property(nonatomic, weak) id<AtMemorySearchResultsViewControllerDelegate>
    delegate;

// The search results to display.
@property(nonatomic, copy) NSArray<AtMemorySearchResultItem*>* results;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_RESULTS_VIEW_CONTROLLER_H_
