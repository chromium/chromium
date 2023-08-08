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

// Invoked before the specified WebState is updated. Currently, this is called
// only before a WebState is detached from WebStateList.
- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)change
                        status:(const WebStateListStatus&)status;

// Invoked after the WebStateList is updated.
- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status;

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
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WillBeginBatchOperation(WebStateList* web_state_list) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;
  void WebStateListDestroyed(WebStateList* web_state_list) final;
  __weak id<WebStateListObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_BRIDGE_H_
