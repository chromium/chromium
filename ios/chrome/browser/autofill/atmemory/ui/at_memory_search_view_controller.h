// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class AtMemorySearchViewController;

@class CrURL;

@protocol AtMemorySearchViewControllerDelegate <NSObject>
// Notifies that the search cell was tapped to start the search.
- (void)searchViewControllerDidTapSearch:
    (AtMemorySearchViewController*)viewController;

// Notifies that the user tapped a link URL.
- (void)searchViewController:(AtMemorySearchViewController*)viewController
               didTapLinkURL:(CrURL*)URL;
@end

@interface AtMemorySearchViewController : ChromeTableViewController

@property(nonatomic, weak) id<AtMemorySearchViewControllerDelegate> delegate;

// Updates the query text shown in the cell.
@property(nonatomic, copy) NSString* query;

// Sets whether the search is loading (showing spinner and updated texts).
@property(nonatomic, assign) BOOL loading;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_VIEW_CONTROLLER_H_
