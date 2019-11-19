// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

// Protocol that correspond to WebStateListObserver API. Allows registering
// Objective-C objects to listen to WebStateList events.
@protocol WebStateListObserving<NSObject>

@optional

// Invoked after a new WebState has been added to the WebStateList at the
// specified index. |activating| will be YES if the WebState will become
// the new active WebState after the insertion.
- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating;

// Invoked after the WebState at the specified index is moved to another index.
- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex;

// Invoked after the WebState at the specified index is replaced by another
// WebState.
- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex;

// Invoked before the specified WebState is detached from the WebStateList.
// The WebState is still valid and still in the WebStateList.
- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)atIndex;

// Invoked after the WebState at the specified index has been detached. The
// WebState is still valid but is no longer in the WebStateList.
- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex;

// Invoked before the specified WebState is destroyed via the WebStateList.
// The WebState is still valid but is no longer in the WebStateList. If the
// WebState is closed due to user action, |userAction| will be true.
- (void)webStateList:(WebStateList*)webStateList
    willCloseWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           userAction:(BOOL)userAction;

// Invoked after |newWebState| was activated at the specified index. Both
// WebState are either valid or null (if there was no selection or there is
// no selection). If |reason| has CHANGE_REASON_USER_ACTION set then the
// change is due to an user action. If |reason| has CHANGE_REASON_REPLACED
// set then the change is caused because the WebState was replaced.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason;

// Invoked before a batched operations begins. The observer can use this
// notification if it is interested in considering all those individual
// operations as a single mutation of the WebStateList (e.g. considering
// insertion of multiple tabs as a restoration operation).
- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList;

// Invoked after the completion of batched operations. The observer can
// investigate the state of the WebStateList to detect any changes that
// were performed on it during the batch (e.g. detect that all tabs were
// closed at once).
- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList;

@end

// Observer that bridges WebStateList events to an Objective-C observer that
// implements the WebStateListObserver protocol (the observer is *not* owned).
class WebStateListObserverBridge final : public WebStateListObserver {
 public:
  explicit WebStateListObserverBridge(id<WebStateListObserving> observer);
  ~WebStateListObserverBridge() final;

 private:
  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) final;
  void WebStateMoved(WebStateList* web_state_list,
                     web::WebState* web_state,
                     int from_index,
                     int to_index) final;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) final;
  void WillDetachWebStateAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index) final;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) final;
  void WillCloseWebStateAt(WebStateList* web_state_list,
                           web::WebState* web_state,
                           int index,
                           bool user_action) final;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           int reason) final;
  void WillBeginBatchOperation(WebStateList* web_state_list) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;

  __weak id<WebStateListObserving> observer_ = nil;

  DISALLOW_COPY_AND_ASSIGN(WebStateListObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_
