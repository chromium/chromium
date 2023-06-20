// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_

#include "base/observer_list_types.h"

class WebStateList;

namespace web {
class WebState;
}

// Represents a generic change to the WebStateList. Use `type()` to determine
// its type, then access the correct sub-class using `As()<...>` method.
class WebStateListChange {
 public:
  enum class Type {
    // Used when the status of a WebState is updated by the activation or the
    // pinned state update. It does not update the layout of WebStateList.
    kSelectionOnly,
    // Used when a WebState at the specified index is detached. The detached
    // WebState is still valid when observer is called but it is no longer in
    // WebStateList.
    kDetach,
    // Used when a WebState at the specified index is moved to a new index.
    kMove,
    // Used when a WebState at the specified index is replaced with a new
    // WebState.
    kReplace,
    // Used when a new WebState is inserted into WebStateList.
    kInsert,
  };

  // Non-copyable, non-moveable.
  WebStateListChange(const WebStateListChange&) = delete;
  WebStateListChange& operator=(const WebStateListChange&) = delete;

  virtual ~WebStateListChange() = default;

  virtual Type type() const = 0;

  template <typename T>
  const T& As() const {
    CHECK(type() == T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  WebStateListChange() = default;
};

// Represents a change that corresponds to detaching one WebState from
// WebStateList.
class WebStateListChangeDetach final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kDetach;

  explicit WebStateListChangeDetach(raw_ptr<web::WebState> detached_web_state);
  ~WebStateListChangeDetach() final = default;

  Type type() const final;

  // The WebState that is detached from WebStateList. The detached WebState is
  // still valid when observer is called but it is no longer in WebStateList at
  // the index position.
  raw_ptr<web::WebState> detached_web_state() const {
    CHECK(detached_web_state_);
    return detached_web_state_;
  }

 private:
  raw_ptr<web::WebState> detached_web_state_;
};

// Represents a change that corresponds to moving one WebState to a new index in
// WebStateList. There is no change in the number of WebStates.
class WebStateListChangeMove final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kMove;

  WebStateListChangeMove(raw_ptr<web::WebState> moved_web_state,
                         int moved_from_index);
  ~WebStateListChangeMove() final = default;

  Type type() const final;

  // The WebState that is moved from the position of `moved_from_index` in
  // WebStateListChangeMove to the position of `index` in WebStateSelection.
  raw_ptr<web::WebState> moved_web_state() const {
    CHECK(moved_web_state_);
    return moved_web_state_;
  }

  // The index of the previous position of a WebState.
  int moved_from_index() const { return moved_from_index_; }

 private:
  raw_ptr<web::WebState> moved_web_state_;
  const int moved_from_index_;
};

// Represents a change that corresponds to replacing one WebState by another
// WebState in-place. There is no change in the number of WebStates.
class WebStateListChangeReplace final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kReplace;

  WebStateListChangeReplace(raw_ptr<web::WebState> replaced_web_state,
                            raw_ptr<web::WebState> inserted_web_state);
  ~WebStateListChangeReplace() final = default;

  Type type() const final;

  // The WebState that is removed from the WebStateList. It
  // is replaced in-place by `inserted_web_state_`.
  raw_ptr<web::WebState> replaced_web_state() const {
    CHECK(replaced_web_state_);
    return replaced_web_state_;
  }

  // The WebState that is inserted into the WebStateList. It
  // takes the position of `replaced_web_state_`.
  raw_ptr<web::WebState> inserted_web_state() const {
    CHECK(inserted_web_state_);
    return inserted_web_state_;
  }

 private:
  raw_ptr<web::WebState> replaced_web_state_;
  raw_ptr<web::WebState> inserted_web_state_;
};

// Represents a change that corresponds to inserting one WebState to
// WebStateList.
class WebStateListChangeInsert final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kInsert;

  explicit WebStateListChangeInsert(raw_ptr<web::WebState> inserted_web_state);
  ~WebStateListChangeInsert() final = default;

  Type type() const final;

  // The WebState that is inserted into the WebStateList. It is inserted to the
  // position of `index` in WebStateSelection.
  raw_ptr<web::WebState> inserted_web_state() const {
    CHECK(inserted_web_state_);
    return inserted_web_state_;
  }

 private:
  raw_ptr<web::WebState> inserted_web_state_;
};

struct WebStateSelection {
  // The index to be changed. A WebState is no longer in WebStateList at the
  // `index` position when a WebState is detached.
  const int index;
  // True when the WebState at `index` is being activated.
  // TODO(crbug.com/1442546): Remove `activating` and introduce `active_index`,
  // the index of the currently active WebState, once WebStateActivatedAt() is
  // merged into WebStateListChange() because WebStateListChange() will be able
  // to handle an operation with the activation at the same time.
  const bool activating;
};

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

  /// Invoked when WebStateList is updated.
  virtual void WebStateListChanged(WebStateList* web_state_list,
                                   const WebStateListChange& change,
                                   const WebStateSelection& selection);

  // Invoked before the specified WebState is detached from the WebStateList.
  // The WebState is still valid and still in the WebStateList.
  virtual void WillDetachWebStateAt(WebStateList* web_state_list,
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

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_
