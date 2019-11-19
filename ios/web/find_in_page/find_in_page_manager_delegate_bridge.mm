// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/find_in_page_manager_delegate_bridge.h"

#import "ios/web/public/find_in_page/find_in_page_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FindInPageManagerDelegateBridge::FindInPageManagerDelegateBridge(
    id<CRWFindInPageManagerDelegate> delegate)
    : delegate_(delegate) {}

FindInPageManagerDelegateBridge::~FindInPageManagerDelegateBridge() {}

void FindInPageManagerDelegateBridge::DidHighlightMatches(WebState* web_state,
                                                          int match_count,
                                                          NSString* query) {
  if ([delegate_ respondsToSelector:@selector
                 (findInPageManager:
                     didHighlightMatchesOfQuery:withMatchCount:forWebState:)]) {
    [delegate_ findInPageManager:web::FindInPageManager::FromWebState(web_state)
        didHighlightMatchesOfQuery:query
                    withMatchCount:match_count
                       forWebState:web_state];
  }
}

void FindInPageManagerDelegateBridge::DidSelectMatch(WebState* web_state,
                                                     int index,
                                                     NSString* context_string) {
  if ([delegate_ respondsToSelector:@selector
                 (findInPageManager:
                     didSelectMatchAtIndex:withContextString:forWebState:)]) {
    [delegate_ findInPageManager:web::FindInPageManager::FromWebState(web_state)
           didSelectMatchAtIndex:index
               withContextString:context_string
                     forWebState:web_state];
  }
}
}
