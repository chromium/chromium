// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_CONTROLLER_H_

#import <Foundation/Foundation.h>

@protocol FindInPageResponseDelegate;
@class FindInPageModel;

namespace web {
class WebState;
}

// The Find in Page controller instantiated by `FindTabHelper` when the
// associated web state is realized, and deinstantiated when it is not.
@interface FindInPageController : NSObject

// Clear search term.
+ (void)clearSearchTerm;

#pragma mark - Properties

// Find In Page model reported to the delegate when the Find session is updated.
@property(nonatomic, readonly, strong) FindInPageModel* findInPageModel;

// FindInPageResponseDelegate instance used to pass back responses to find
// actions.
@property(nonatomic, weak) id<FindInPageResponseDelegate> responseDelegate;

#pragma mark - Initialization/deinitialization

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

// Instructs the controller to detach itself from the web state.
- (void)detachFromWebState;

#pragma mark - Saving/restoring search term

// Save search term to global variable.
- (void)saveSearchTerm;

// Restore search term from global variable, updating `findInPageModel`.
- (void)restoreSearchTerm;

#pragma mark - Find actions

// Whether a Find session can be started on the current page.
- (BOOL)canFindInPage;

// Start a Find operation on the web state with the given `query`.
- (void)findStringInPage:(NSString*)query;

// Move to the next find result.
- (void)findNextStringInPage;

// Move to the previous find result.
- (void)findPreviousStringInPage;

// Stop any ongoing Find session.
- (void)disableFindInPage;

@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_CONTROLLER_H_
