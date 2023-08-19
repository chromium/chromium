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

// WebStateListChange and WebStateListStatus are used to inform
// WebStateListObservers of changes of
// (1) inserted/remove/replaced/removed WebStates
// (2) status updates
//    (2-1) activated WebState
//    (2-2) pinned/non-pinned WebState
// The change of (1) and (2) can be triggered synchronously.

// Represents a generic change to the WebStateList. Use `type()` to determine
// its type, then access the correct sub-class using `As()<...>` method.
class WebStateListChange {
 public:
  enum class Type {
    // Used when the status of a WebState is updated by the activation or the
    // pinned state update. It does not update the layout of WebStateList.
    kStatusOnly,
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

// Represents a change that corresponds to updating the active state or the
// pinned state of a selected WebState.
class WebStateListChangeStatusOnly final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kStatusOnly;

  explicit WebStateListChangeStatusOnly(
      raw_ptr<web::WebState> selected_web_state);
  ~WebStateListChangeStatusOnly() final = default;

  Type type() const final;

  // The WebState that is updated. The selected WebState updates the active
  // state or the pinned state, but the position of it isn't updated in
  // WebStateList.
  raw_ptr<web::WebState> selected_web_state() const {
    CHECK(selected_web_state_);
    return selected_web_state_;
  }

 private:
  raw_ptr<web::WebState> selected_web_state_;
};

// Represents a change that corresponds to detaching one WebState from
// WebStateList.
class WebStateListChangeDetach final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kDetach;

  WebStateListChangeDetach(raw_ptr<web::WebState> detached_web_state,
                           bool is_closing,
                           bool is_user_action);
  ~WebStateListChangeDetach() final = default;

  Type type() const final;

  // The WebState that is detached from WebStateList. The detached WebState is
  // still valid when observer is called but it is no longer in WebStateList at
  // the index position.
  raw_ptr<web::WebState> detached_web_state() const {
    CHECK(detached_web_state_);
    return detached_web_state_;
  }

  // Returns true when a detached WebState will be closed as well.
  bool is_closing() const { return is_closing_; }
  // Returns true when a detached WebState will be closed by the user action.
  bool is_user_action() const { return is_user_action_; }

 private:
  raw_ptr<web::WebState> detached_web_state_;
  const bool is_closing_;
  const bool is_user_action_;
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
  // WebStateListChangeMove to the position of `index` in
  // WebStateListStatus.
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
  // position of `index` in WebStateListStatus.
  raw_ptr<web::WebState> inserted_web_state() const {
    CHECK(inserted_web_state_);
    return inserted_web_state_;
  }

 private:
  raw_ptr<web::WebState> inserted_web_state_;
};

// Represents a change on pinned state/activation.
struct WebStateListStatus {
  // The index to be changed. A WebState is no longer in WebStateList at the
  // `index` position when a WebState is detached.
  const int index;
  // True when the pinned state of the WebState at `index` in WebStateList is
  // updated.
  const bool pinned_state_change;
  // WebState that used to be active before the change in WebStateList is
  // finished.
  const raw_ptr<web::WebState> old_active_web_state;
  // WebState that will be active after the change in WebStateList is finished.
  const raw_ptr<web::WebState> new_active_web_state;
  // True when the active WebState is updated.
  bool active_web_state_change() const {
    return old_active_web_state != new_active_web_state;
  }
};

// Interface for listening to events occurring to WebStateLists.
class WebStateListObserver : public base::CheckedObserver {
 public:
  WebStateListObserver();

  WebStateListObserver(const WebStateListObserver&) = delete;
  WebStateListObserver& operator=(const WebStateListObserver&) = delete;

  ~WebStateListObserver() override;

  // Invoked before the specified WebState is updated. Is is currently used to
  // notify the event before a WebState is detached from WebStateList. So the
  // type of `change` is always `WebStateListChangeDetach`.
  virtual void WebStateListWillChange(
      WebStateList* web_state_list,
      const WebStateListChangeDetach& detach_change,
      const WebStateListStatus& status);

  /// Invoked when WebStateList is updated.
  virtual void WebStateListDidChange(WebStateList* web_state_list,
                                     const WebStateListChange& change,
                                     const WebStateListStatus& status);

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
