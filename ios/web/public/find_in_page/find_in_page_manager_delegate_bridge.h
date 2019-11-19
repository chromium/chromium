// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_BRIDGE_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"

namespace web {
class FindInPageManager;
}

// Objective-C interface for web::FindInPageManagerDelegate
@protocol CRWFindInPageManagerDelegate <NSObject>
// Called when a search for |query| finished with |match_count| found and all
// matches were highlighted after calling FindInPageManager::Find() with
// FindInPageSearch. Even if no matches are found, call will be made once a
// find has completed, assuming it has not been interrupted by another find.
// Will also be called if the total match count in the current page changes.
// Client should check |query| to ensure that it is processing |match_count|
// for the correct find.
- (void)findInPageManager:(web::FindInPageManager*)manager
    didHighlightMatchesOfQuery:(NSString*)query
                withMatchCount:(NSInteger)matchCount
                   forWebState:(web::WebState*)webState;
// Called when a match number |index| is selected with |contextString|
// representing the textual context of the match. |contextString| can be used
// in VoiceOver to notify the user of the context of the match in the sentence.
// A selected match refers to a match that is highlighted in a unique manner
// different from the other matches. This is triggered by calling
// FindInPageManager::Find() with any FindInPageOptions to indicate the new
// match number that was selected. This method is not called if
// |FindInPageManager::Find| did not find any matches.
- (void)findInPageManager:(web::FindInPageManager*)manager
    didSelectMatchAtIndex:(NSInteger)index
        withContextString:(NSString*)contextString
              forWebState:(web::WebState*)webState;
@end

namespace web {

class WebState;

// Adapter to use an id<CRWFindInPageManagerDelegate> as a
// web::FindInPageManagerDelegate.
class FindInPageManagerDelegateBridge : public web::FindInPageManagerDelegate {
 public:
  explicit FindInPageManagerDelegateBridge(
      id<CRWFindInPageManagerDelegate> delegate);
  ~FindInPageManagerDelegateBridge() override;

  // FindInPageManagerDelegate overrides.
  void DidHighlightMatches(WebState* web_state,
                           int match_count,
                           NSString* query) override;
  void DidSelectMatch(WebState* web_state,
                      int index,
                      NSString* context_string) override;

 private:
  __weak id<CRWFindInPageManagerDelegate> delegate_ = nil;
  DISALLOW_COPY_AND_ASSIGN(FindInPageManagerDelegateBridge);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_BRIDGE_H_
