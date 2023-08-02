// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_find_in_page_manager_delegate.h"

@implementation CRWFakeFindInPageManagerDelegate

- (void)findInPageManager:(web::FindInPageManager*)manager
    didHighlightMatchesOfQuery:(NSString*)query
                withMatchCount:(NSInteger)matchCount
                   forWebState:(web::WebState*)webState {
  _webState = webState;
  _matchCount = matchCount;
  _query = [query copy];
}

- (void)findInPageManager:(web::FindInPageManager*)manager
    didSelectMatchAtIndex:(NSInteger)index
        withContextString:(NSString*)contextString
              forWebState:(web::WebState*)webState {
  _webState = webState;
  _contextString = [contextString copy];
  _index = index;
}

- (void)userDismissedFindNavigatorForManager:
    (web::AbstractFindInPageManager*)manager {
  _userDismissedFindNavigator = YES;
}

@end
