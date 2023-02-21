// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_

#include "base/observer_list_types.h"

class WebStateList;

namespace web {
class WebState;
}

// Constants used when notifying about changes to active WebState.
enum class ActiveWebStateChangeReason {
  // Used to indicate the active WebState changed because active WebState was
  // replaced (e.g. a pre-rendered WebState is promoted to a real tab).
  Replaced,

  // Used to indicate the active WebState changed because it was activated.
  Activated,

  // Used to indicate the active WebState changed because active WebState was
  // closed (or detached in case of multi-window).
  Closed,

  // Used to indicate the active WebState changed because a new active
  // WebState was inserted (e.g. the first WebState is created).
  Inserted,
};

// Interface for listening to events occurring to WebStateLists.
class WebStateListObserver : public base::CheckedObserver {
 public:
  WebStateListObserver();

  WebStateListObserver(const WebStateListObserver&) = delete;
  WebStateListObserver& operator=(const WebStateListObserver&) = delete;

  ~WebStateListObserver() override;

  // Invoked after a new WebState has been added to the WebStateList at the
  // specified index. `activating` will be true if the WebState will become
  // the new active WebState after the insertion.
  virtual void WebStateInsertedAt(WebStateList* web_state_list,
                                  web::WebState* web_state,
                                  int index,
                                  bool activating);

  // Invoked after the WebState at the specified index is moved to another
  // index.
  virtual void WebStateMoved(WebStateList* web_state_list,
                             web::WebState* web_state,
                             int from_index,
                             int to_index);

  // Invoked after the WebState at the specified index is replaced by another
  // WebState.
  virtual void WebStateReplacedAt(WebStateList* web_state_list,
                                  web::WebState* old_web_state,
                                  web::WebState* new_web_state,
                                  int index);

  // Invoked before the specified WebState is detached from the WebStateList.
  // The WebState is still valid and still in the WebStateList.
  virtual void WillDetachWebStateAt(WebStateList* web_state_list,
                                    web::WebState* web_state,
                                    int index);

  // Invoked after the WebState at the specified index has been detached. The
  // WebState is still valid but is no longer in the WebStateList.
  virtual void WebStateDetachedAt(WebStateList* web_state_list,
                                  web::WebState* web_state,
                                  int index);

  // Invoked before the specified WebState is destroyed via the WebStateList.
  // The WebState is still valid but is no longer in the WebStateList. If the
  // WebState is closed due to user action, `user_action` will be true.
  virtual void WillCloseWebStateAt(WebStateList* web_state_list,
                                   web::WebState* web_state,
                                   int index,
                                   bool user_action);

  // Invoked after `new_web_state` was activated at the specified index. Both
  // WebState are either valid or null (if there was no selection or there is
  // no selection). See ChangeReason enum for possible values for `reason`.
  virtual void WebStateActivatedAt(WebStateList* web_state_list,
                                   web::WebState* old_web_state,
                                   web::WebState* new_web_state,
                                   int active_index,
                                   ActiveWebStateChangeReason reason);

  // Invoked when the pinned state of a tab changes.
  virtual void WebStatePinnedStateChanged(WebStateList* web_state_list,
                                          web::WebState* web_state,
                                          int index);

  // Invoked before a batched operations begins. The observer can use this
  // notification if it is interested in considering all those individual
  // operations as a single mutation of the WebStateList (e.g. considering
  // insertion of multiple tabs as a restoration operation).
  virtual void WillBeginBatchOperation(WebStateList* web_state_list);

  // Invoked after the completion of batched operations. The observer can
  // investigate the state of the WebStateList to detect any changes that
  // were performed on it during the batch (e.g. detect that all tabs were
  // closed at once).
  virtual void BatchOperationEnded(WebStateList* web_state_list);

  // Invoked when the WebStateList is being destroyed. Gives subclasses a chance
  // to cleanup.
  virtual void WebStateListDestroyed(WebStateList* web_state_list);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_
