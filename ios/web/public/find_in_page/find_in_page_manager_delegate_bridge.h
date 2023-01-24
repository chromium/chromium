// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_BRIDGE_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/find_in_page/find_in_page_manager_delegate.h"

namespace web {
class AbstractFindInPageManager;
}

// Objective-C interface for web::FindInPageManagerDelegate
@protocol CRWFindInPageManagerDelegate <NSObject>
// Called when a search for `query` finished with `match_count` found and all
// matches were highlighted after calling FindInPageManager::Find() with
// FindInPageSearch. Even if no matches are found, call will be made once a
// find has completed, assuming it has not been interrupted by another find.
// Will also be called if the total match count in the current page changes.
// Client should check `query` to ensure that it is processing `match_count`
// for the correct find.
- (void)findInPageManager:(web::AbstractFindInPageManager*)manager
    didHighlightMatchesOfQuery:(NSString*)query
                withMatchCount:(NSInteger)matchCount
                   forWebState:(web::WebState*)webState;
// Called when a match number `index` is selected with `contextString`
// representing the textual context of the match. `contextString` can be used
// in VoiceOver to notify the user of the context of the match in the sentence.
// A selected match refers to a match that is highlighted in a unique manner
// different from the other matches. This is triggered by calling
// FindInPageManager::Find() with any FindInPageOptions to indicate the new
// match number that was selected. This method is not called if
// `FindInPageManager::Find` did not find any matches.
- (void)findInPageManager:(web::AbstractFindInPageManager*)manager
    didSelectMatchAtIndex:(NSInteger)index
        withContextString:(NSString*)contextString
              forWebState:(web::WebState*)webState;

// Called when the Find in Page manager detects the Find session has been ended
// by the user through the system Find panel.
- (void)userDismissedFindNavigatorForManager:
    (web::AbstractFindInPageManager*)manager;

@end

namespace web {

class WebState;

// Adapter to use an id<CRWFindInPageManagerDelegate> as a
// web::FindInPageManagerDelegate.
class FindInPageManagerDelegateBridge : public web::FindInPageManagerDelegate {
 public:
  explicit FindInPageManagerDelegateBridge(
      id<CRWFindInPageManagerDelegate> delegate);

  FindInPageManagerDelegateBridge(const FindInPageManagerDelegateBridge&) =
      delete;
  FindInPageManagerDelegateBridge& operator=(
      const FindInPageManagerDelegateBridge&) = delete;

  ~FindInPageManagerDelegateBridge() override;

  // FindInPageManagerDelegate overrides.
  void DidHighlightMatches(AbstractFindInPageManager* manager,
                           WebState* web_state,
                           int match_count,
                           NSString* query) override;
  void DidSelectMatch(AbstractFindInPageManager* manager,
                      WebState* web_state,
                      int index,
                      NSString* context_string) override;
  void UserDismissedFindNavigator(AbstractFindInPageManager* manager) override;

 private:
  __weak id<CRWFindInPageManagerDelegate> delegate_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_BRIDGE_H_
