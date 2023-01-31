// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/find_in_page/find_in_page_manager_delegate_bridge.h"

namespace web {
class FindInPageManager;
class WebState;
}

// Class which conforms to CRWFindInPageManagerDelegate protocol.
@interface CRWFakeFindInPageManagerDelegate
    : NSObject <CRWFindInPageManagerDelegate>

// CRWFindInPageManagerDelegate methods.
- (void)findInPageManager:(web::FindInPageManager*)manager
    didHighlightMatchesOfQuery:(NSString*)query
                withMatchCount:(NSInteger)matchCount
                   forWebState:(web::WebState*)webState;
- (void)findInPageManager:(web::FindInPageManager*)manager
    didSelectMatchAtIndex:(NSInteger)index
        withContextString:(NSString*)contextString
              forWebState:(web::WebState*)webState;
- (void)userDismissedFindNavigatorForManager:
    (web::AbstractFindInPageManager*)manager;

// The last web::WebState received in delegate method calls.
@property(nonatomic, readonly) web::WebState* webState;
// The last `query` string passed in `didHighlightMatchesOfQuery:`.
@property(nonatomic, readonly) NSString* query;
// The last `matchCount` passed in `didHighlightMatchesOfQuery:`.
@property(nonatomic, readonly) NSInteger matchCount;
// The last `index` passed in `didSelectMatchAtIndex:`.
@property(nonatomic, readonly) NSInteger index;
// The last `contextString` passed in `didSelectMatchAtIndex:`.
@property(nonatomic, readonly) NSString* contextString;
// Whether `userDismissedFindNavigatorForManager` has been called.
@property(nonatomic, readonly) BOOL userDismissedFindNavigator;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
