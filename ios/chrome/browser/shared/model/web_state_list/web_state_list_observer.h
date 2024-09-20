// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_OBSERVER_H_

#import "base/observer_list_types.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class TabGroup;

namespace web {
class WebState;
}

// WebStateListChange and WebStateListStatus are used to inform
// WebStateListObservers of changes of
// (1) inserted/remove/replaced/removed WebStates
// (2) status updates
//    (2-1) activated WebState
//    (2-2) pinned/non-pinned WebState
//    (2-3) grouped/ungrouped WebState
// The change of (1) and (2) can be triggered synchronously.

// Represents a generic change to the WebStateList. Use `type()` to determine
// its type, then access the correct sub-class using `As()<...>` method.
class WebStateListChange {
 public:
  enum class Type {
    // Used when the status of a WebState is updated by (de)activation,
    // (un)pinning, (un)grouping. It does not update the layout of WebStateList.
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
    // Used when a tab group is created.
    kGroupCreate,
    // Used when a group's visual data were updated.
    kGroupVisualDataUpdate,
    // Used when a tab group at the specified range is moved to a new range.
    kGroupMove,
    // Used when a tab group is deleted.
    kGroupDelete,
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

// Represents a change that corresponds to updating the active state, the
// pinned state, or the group state of a given WebState.
class WebStateListChangeStatusOnly final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kStatusOnly;

  explicit WebStateListChangeStatusOnly(raw_ptr<web::WebState> web_state,
                                        int index,
                                        bool pinned_state_changed,
                                        raw_ptr<const TabGroup> old_group,
                                        raw_ptr<const TabGroup> new_group);
  ~WebStateListChangeStatusOnly() final = default;

  Type type() const final;

  // The WebState that is updated. Its active state, pinned state, or group
  // state could have been updated, but not its position in WebStateList.
  raw_ptr<web::WebState> web_state() const {
    CHECK(web_state_);
    return web_state_;
  }

  // Returns the current index of the WebState.
  int index() const { return index_; }

  // Returns whether the pinned state of the WebState changed.
  bool pinned_state_changed() const { return pinned_state_changed_; }

  // The group the WebState was in prior to the change.
  raw_ptr<const TabGroup> old_group() const { return old_group_; }

  // The group the WebState is now in.
  raw_ptr<const TabGroup> new_group() const { return new_group_; }

 private:
  raw_ptr<web::WebState> web_state_;
  const int index_;
  const bool pinned_state_changed_;
  raw_ptr<const TabGroup> old_group_;
  raw_ptr<const TabGroup> new_group_;
};

// Represents a change that corresponds to detaching one WebState from
// WebStateList.
class WebStateListChangeDetach final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kDetach;

  // TODO(crbug.com/365701685): Refactor WebStateListChangeDetach to use an
  // enum for the reason why a WebState is being detached.
  WebStateListChangeDetach(raw_ptr<web::WebState> detached_web_state,
                           int detached_from_index,
                           bool is_closing,
                           bool is_user_action,
                           bool is_tabs_cleanup,
                           raw_ptr<const TabGroup> group);
  ~WebStateListChangeDetach() final = default;

  Type type() const final;

  // The WebState that is detached from WebStateList. The detached WebState is
  // still valid when observer is called but it is no longer in WebStateList at
  // the index position.
  raw_ptr<web::WebState> detached_web_state() const {
    CHECK(detached_web_state_);
    return detached_web_state_;
  }

  // Returns the index of the WebState was in before being detached.
  int detached_from_index() const { return detached_from_index_; }

  // Returns true when a detached WebState will be closed as well.
  bool is_closing() const { return is_closing_; }

  // Returns true when a detached WebState will be closed by the user action.
  bool is_user_action() const { return is_user_action_; }

  // TODO(crbug.com/365701685): Refactor WebStateListChangeDetach to use an
  // enum for the reason why a WebState is being detached.
  // Returns true when a detached WebState will be closed in a tabs clean-up.
  bool is_tabs_cleanup() const { return is_tabs_cleanup_; }

  // The group the WebState was in prior to the change.
  raw_ptr<const TabGroup> group() const { return group_; }

 private:
  raw_ptr<web::WebState> detached_web_state_;
  const int detached_from_index_;
  const bool is_closing_;
  const bool is_user_action_;
  const bool is_tabs_cleanup_;
  raw_ptr<const TabGroup> group_;
};

// Represents a change that corresponds to moving one WebState to a new index in
// WebStateList. There is no change in the number of WebStates.
class WebStateListChangeMove final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kMove;

  WebStateListChangeMove(raw_ptr<web::WebState> moved_web_state,
                         int moved_from_index,
                         int moved_to_index,
                         bool pinned_state_changed,
                         raw_ptr<const TabGroup> old_group,
                         raw_ptr<const TabGroup> new_group);
  ~WebStateListChangeMove() final = default;

  Type type() const final;

  // The WebState that is moved from `moved_from_index` to `moved_to_index`.
  raw_ptr<web::WebState> moved_web_state() const {
    CHECK(moved_web_state_);
    return moved_web_state_;
  }

  // The index of the previous position of the WebState.
  int moved_from_index() const { return moved_from_index_; }

  // The index of the current position of the WebState.
  int moved_to_index() const { return moved_to_index_; }

  // Returns whether the pinned state of the WebState changed.
  bool pinned_state_changed() const { return pinned_state_changed_; }

  // The group the WebState was in prior to the change.
  raw_ptr<const TabGroup> old_group() const { return old_group_; }

  // The group the WebState is now in.
  raw_ptr<const TabGroup> new_group() const { return new_group_; }

 private:
  raw_ptr<web::WebState> moved_web_state_;
  const int moved_from_index_;
  const int moved_to_index_;
  const bool pinned_state_changed_;
  raw_ptr<const TabGroup> old_group_;
  raw_ptr<const TabGroup> new_group_;
};

// Represents a change that corresponds to replacing one WebState by another
// WebState in-place. There is no change in the number of WebStates.
class WebStateListChangeReplace final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kReplace;

  WebStateListChangeReplace(raw_ptr<web::WebState> replaced_web_state,
                            raw_ptr<web::WebState> inserted_web_state,
                            int index);
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

  // Returns the current index of the WebState.
  int index() const { return index_; }

 private:
  raw_ptr<web::WebState> replaced_web_state_;
  raw_ptr<web::WebState> inserted_web_state_;
  const int index_;
};

// Represents a change that corresponds to inserting one WebState to
// WebStateList.
class WebStateListChangeInsert final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kInsert;

  explicit WebStateListChangeInsert(raw_ptr<web::WebState> inserted_web_state,
                                    int index,
                                    raw_ptr<const TabGroup> group);
  ~WebStateListChangeInsert() final = default;

  Type type() const final;

  // The WebState that is inserted into the WebStateList. It is inserted to the
  // position of `index` in WebStateListStatus.
  raw_ptr<web::WebState> inserted_web_state() const {
    CHECK(inserted_web_state_);
    return inserted_web_state_;
  }

  // Returns the current index of the WebState.
  int index() const { return index_; }

  // The group the WebState is now in.
  raw_ptr<const TabGroup> group() const { return group_; }

 private:
  raw_ptr<web::WebState> inserted_web_state_;
  const int index_;
  raw_ptr<const TabGroup> group_;
};

// Represents a change that corresponds to the creation of a tab group. The
// group has no WebStates in it, but will soon get some, which will be
// communicated via following Move or StatusOnly changes per WebState.
class WebStateListChangeGroupCreate final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kGroupCreate;

  WebStateListChangeGroupCreate(raw_ptr<const TabGroup> created_group);
  ~WebStateListChangeGroupCreate() final = default;

  Type type() const final;

  // The group that was created.
  raw_ptr<const TabGroup> created_group() const {
    CHECK(created_group_);
    return created_group_;
  }

 private:
  raw_ptr<const TabGroup> created_group_;
};

// Represents a change that corresponds to updating a tab group's visual data.
class WebStateListChangeGroupVisualDataUpdate final
    : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kGroupVisualDataUpdate;

  explicit WebStateListChangeGroupVisualDataUpdate(
      raw_ptr<const TabGroup> updated_group,
      const tab_groups::TabGroupVisualData& old_visual_data);
  ~WebStateListChangeGroupVisualDataUpdate() final = default;

  Type type() const final;

  // The group whose visual data got update.
  raw_ptr<const TabGroup> updated_group() const { return updated_group_; }

  // Returns the previous visual data.
  const tab_groups::TabGroupVisualData old_visual_data() const {
    return old_visual_data_;
  }

 private:
  raw_ptr<const TabGroup> updated_group_;
  const tab_groups::TabGroupVisualData old_visual_data_;
};

// Represents a change that corresponds to moving an entire tab group to a new
// index in WebStateList. There is no change in the number of WebStates, nor to
// the number of WebStates in any group.
class WebStateListChangeGroupMove final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kGroupMove;

  WebStateListChangeGroupMove(raw_ptr<const TabGroup> moved_group,
                              TabGroupRange moved_from_range,
                              TabGroupRange moved_to_range);
  ~WebStateListChangeGroupMove() final = default;

  Type type() const final;

  // The group that is moved from `moved_from_index` to `moved_to_index`.
  raw_ptr<const TabGroup> moved_group() const {
    CHECK(moved_group_);
    return moved_group_;
  }

  // The previous range of the group.
  TabGroupRange moved_from_range() const { return moved_from_range_; }

  // The current range of the group.
  TabGroupRange moved_to_range() const { return moved_to_range_; }

 private:
  raw_ptr<const TabGroup> moved_group_;
  const TabGroupRange moved_from_range_;
  const TabGroupRange moved_to_range_;
};

// Represents a change that corresponds to the deletion of a tab group. The
// group has no WebStates in it anymore, which were communicated via previous
// Move or StatusOnly changes per WebState.
// The pointer to the group must not be reused after this notification.
class WebStateListChangeGroupDelete final : public WebStateListChange {
 public:
  static constexpr Type kType = Type::kGroupDelete;

  WebStateListChangeGroupDelete(raw_ptr<const TabGroup> deleted_group);
  ~WebStateListChangeGroupDelete() final = default;

  Type type() const final;

  // The group that was deleted.
  raw_ptr<const TabGroup> deleted_group() const {
    CHECK(deleted_group_);
    return deleted_group_;
  }

 private:
  raw_ptr<const TabGroup> deleted_group_;
};

// Represents what changed during a WebStateListChange for a given WebState.
struct WebStateListStatus {
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

  // Invoked before the specified WebStateList is updated. It is currently used
  // to notify the event before a WebState is detached from WebStateList. So the
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
