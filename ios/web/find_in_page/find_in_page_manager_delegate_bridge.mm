// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/find_in_page_manager_delegate_bridge.h"

#import "ios/web/public/find_in_page/java_script_find_in_page_manager.h"

namespace web {

FindInPageManagerDelegateBridge::FindInPageManagerDelegateBridge(
    id<CRWFindInPageManagerDelegate> delegate)
    : delegate_(delegate) {}

FindInPageManagerDelegateBridge::~FindInPageManagerDelegateBridge() {}

void FindInPageManagerDelegateBridge::DidHighlightMatches(
    AbstractFindInPageManager* manager,
    WebState* web_state,
    int match_count,
    NSString* query) {
  if ([delegate_ respondsToSelector:@selector
                 (findInPageManager:
                     didHighlightMatchesOfQuery:withMatchCount:forWebState:)]) {
    [delegate_ findInPageManager:manager
        didHighlightMatchesOfQuery:query
                    withMatchCount:match_count
                       forWebState:web_state];
  }
}

void FindInPageManagerDelegateBridge::DidSelectMatch(
    AbstractFindInPageManager* manager,
    WebState* web_state,
    int index,
    NSString* context_string) {
  if ([delegate_ respondsToSelector:@selector
                 (findInPageManager:
                     didSelectMatchAtIndex:withContextString:forWebState:)]) {
    [delegate_ findInPageManager:manager
           didSelectMatchAtIndex:index
               withContextString:context_string
                     forWebState:web_state];
  }
}

void FindInPageManagerDelegateBridge::UserDismissedFindNavigator(
    AbstractFindInPageManager* manager) {
  if ([delegate_ respondsToSelector:@selector
                 (userDismissedFindNavigatorForManager:)]) {
    [delegate_ userDismissedFindNavigatorForManager:manager];
  }
}

}  // namespace web
