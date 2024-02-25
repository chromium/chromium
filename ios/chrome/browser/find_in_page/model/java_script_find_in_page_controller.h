// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_JAVA_SCRIPT_FIND_IN_PAGE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_JAVA_SCRIPT_FIND_IN_PAGE_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

namespace web {
class WebState;
}

@class FindInPageModel;
@protocol FindInPageResponseDelegate;

@interface JavaScriptFindInPageController : NSObject

// Find In Page model.
@property(nonatomic, readonly, strong) FindInPageModel* findInPageModel;
// FindInPageResponseDelegate instance used to pass back responses to find
// actions.
@property(nonatomic, weak) id<FindInPageResponseDelegate> responseDelegate;

// Designated initializer.
- (id)initWithWebState:(web::WebState*)webState;
// Is Find In Page available right now (given the state of the WebState)?
- (BOOL)canFindInPage;
// Find `query` in page, update model with results of find. Calls
// `completionHandler` after the find operation is complete. `completionHandler`
// can be nil.
- (void)findStringInPage:(NSString*)query;
// Move to the next find result based on `-findInPageModel`, and scroll to
// match.
- (void)findNextStringInPage;
// Move to the previous find result based on `-findInPageModel`.
- (void)findPreviousStringInPage;
// Disable find in page script and model. `responseDelegate` will be used to
// respond. Cleanup is not guaranteed to be finished when `responseDelegate`
// receives a response.
- (void)disableFindInPage;

// Save search term to Paste UIPasteboard.
- (void)saveSearchTerm;
// Restore search term from Paste UIPasteboard, updating findInPageModel.
- (void)restoreSearchTerm;

// Instructs the controller to detach itself from the web state.
- (void)detachFromWebState;

// Sets the search term to `string`. Stored until the application quit.
+ (void)setSearchTerm:(NSString*)string;
// The search term, stored until the application quit.
+ (NSString*)searchTerm;
@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_JAVA_SCRIPT_FIND_IN_PAGE_CONTROLLER_H_
