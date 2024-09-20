// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_WEB_STATE_LIST_OBSERVER_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class WebStateList;

namespace web {
class WebState;
class WebStateID;
}  // namespace web

// A WebStateListObserver that track the dirty state of the WebStateList and
// of the WebStates stored in it. It observes multiple events that indicate
// the content of the WebStateList has changed (insertion, change of active
// WebState, ...), or the content displayed in any of the contained WebStates
// changed.
class SessionRestorationWebStateListObserver final
    : public WebStateListObserver {
 public:
  // Callback passed upon construction. Will be invoked when the state of
  // either the WebStateList or any of the stored WebState transition from
  // clean to dirty (i.e. needs saving).
  using WebStateListDirtyCallback =
      base::RepeatingCallback<void(WebStateList*)>;

  SessionRestorationWebStateListObserver(WebStateList* web_state_list,
                                         WebStateListDirtyCallback callback);
  ~SessionRestorationWebStateListObserver() final;

  // Returns whether the state of the WebStateList needs to be saved to disk.
  bool is_web_state_list_dirty() const { return is_web_state_list_dirty_; }

  // Returns the set of all dirty WebStates stored in the WebStateList.
  const std::set<web::WebState*> dirty_web_states() const {
    return dirty_web_states_;
  }

  // Returns the set of identifiers of detached WebState that may be adopted
  // by other WebStateList (i.e. whose state on disk is still up-to-date).
  const std::set<web::WebStateID>& detached_web_states() const {
    return detached_web_states_;
  }

  // Returns the set of identifiers of inserted WebState that needs to be
  // adopted by this WebStateList (i.e. whose state cannot be serialized
  // and instead need to be copied from another WebStateList's state on
  // disk).
  const std::set<web::WebStateID>& inserted_web_states() const {
    return inserted_web_states_;
  }

  // Returns the set of identifiers of detached WebState that are scheduled
  // to be closed (i.e. they cannot be adopted and their state on disk can
  // be deleted).
  const std::set<web::WebStateID>& closed_web_states() const {
    return closed_web_states_;
  }

  // Records that an unrealized WebState with `expected_web_state_id` is
  // expected to be inserted in the observed WebStateList soon, and that
  // when inserted the WebState does not have to be adopted.
  void AddExpectedWebState(web::WebStateID expected_web_state_id);

  // Should be called after saving the state of the WebStateList and of
  // the WebStates to disk. The callback passed to the constructor won't
  // be called again until this method is called.
  void ClearDirty();

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;
  void WillBeginBatchOperation(WebStateList* web_state_list) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;
  void WebStateListDestroyed(WebStateList* web_state_list) final;

 private:
  // Helper method invoked when a WebState is detached from the WebStateList.
  void DetachWebState(web::WebState* detached_web_state, bool is_closing);

  // Helper method invoked when a WebState is inserted into the WebStateList.
  void AttachWebState(web::WebState* attached_web_state);

  // Helper method invoked by SessionRestorationWebStateObserver when any of
  // the WebState stored in the WebStateList become dirty. May invoke the
  // callback passed to the constructor.
  void MarkWebStateDirty(web::WebState* web_state);

  // Helper method invoked when the state of the WebStateList is needs to be
  // saved to disk. May invoke the callback passed to the constructor
  void MarkDirty();

  const raw_ptr<WebStateList> web_state_list_;
  WebStateListDirtyCallback callback_;

  bool is_web_state_list_dirty_ = false;
  std::set<web::WebState*> dirty_web_states_;
  std::set<web::WebStateID> detached_web_states_;
  std::set<web::WebStateID> inserted_web_states_;
  std::set<web::WebStateID> expected_web_states_;
  std::set<web::WebStateID> closed_web_states_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_WEB_STATE_LIST_OBSERVER_H_
