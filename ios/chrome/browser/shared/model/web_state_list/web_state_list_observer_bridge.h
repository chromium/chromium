// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

// Protocol that correspond to WebStateListObserver API. Allows registering
// Objective-C objects to listen to WebStateList events.
@protocol WebStateListObserving <NSObject>

@optional

// Invoked after the WebStateList is updated.
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                    selection:(const WebStateSelection&)selection;

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
// WebState is closed due to user action, `userAction` will be true.
- (void)webStateList:(WebStateList*)webStateList
    willCloseWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           userAction:(BOOL)userAction;

// Invoked after `newWebState` was activated at the specified index. Both
// WebState are either valid or null (if there was no selection or there is
// no selection). See ChangeReason enum for possible values for `reason`.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason;

// Invoked after pinned state for `webState` at the specified index has been
// changed.
- (void)webStateList:(WebStateList*)webStateList
    didChangePinnedStateForWebState:(web::WebState*)webState
                            atIndex:(int)atIndex;

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

// Invoked when the WebStateList is being destroyed. Gives subclasses a chance
// to cleanup.
- (void)webStateListDestroyed:(WebStateList*)webStateList;

@end

// Observer that bridges WebStateList events to an Objective-C observer that
// implements the WebStateListObserver protocol (the observer is *not* owned).
class WebStateListObserverBridge final : public WebStateListObserver {
 public:
  explicit WebStateListObserverBridge(id<WebStateListObserving> observer);

  WebStateListObserverBridge(const WebStateListObserverBridge&) = delete;
  WebStateListObserverBridge& operator=(const WebStateListObserverBridge&) =
      delete;

  ~WebStateListObserverBridge() final;

 private:
  // WebStateListObserver implementation.
  void WebStateListChanged(WebStateList* web_state_list,
                           const WebStateListChange& change,
                           const WebStateSelection& selection) override;
  void WillDetachWebStateAt(WebStateList* web_state_list,
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
                           ActiveWebStateChangeReason reason) final;
  void WebStatePinnedStateChanged(WebStateList* web_state_list,
                                  web::WebState* web_state,
                                  int index) final;
  void WillBeginBatchOperation(WebStateList* web_state_list) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;
  void WebStateListDestroyed(WebStateList* web_state_list) final;
  __weak id<WebStateListObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_
