// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#include "url/gurl.h"

class RemovingIndexes;
class TabGroup;
class WebStateListDelegate;
class WebStateListObserver;

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

namespace web {
class WebState;
}  // namespace web

// Manages a list of WebStates.
//
// This class supports mutating the list and to observe the mutations via the
// WebStateListObserver interface. However, the class is not re-entrant, thus
// it is an error to mutate the list from an observer (either directly by the
// observer, or indirectly via code invoked from the observer).
//
// The WebStateList takes ownership of the WebStates that it manages.
//
// WebStates can be pinned, grouped, or ungrouped, which are mutually exclusive
// states. Pinned tabs are always at the beginning of the list.
class WebStateList {
 public:
  // Parameters used when inserting WebStates.
  struct InsertionParams {
    // Lets the WebStateList decide where to insert the WebState.
    static InsertionParams Automatic() { return {}; }

    // Provides the WebStateList with a desired index where to insert the
    // WebState.
    static InsertionParams AtIndex(int desired_index) {
      InsertionParams params;
      params.desired_index = desired_index;
      return params;
    }

    InsertionParams(const InsertionParams&);
    InsertionParams& operator=(const InsertionParams&);
    InsertionParams(InsertionParams&&);
    InsertionParams& operator=(InsertionParams&&);

    WebStateOpener opener;
    raw_ptr<const TabGroup> in_group = nullptr;
    int desired_index = kInvalidIndex;
    bool inherit_opener = false;
    bool activate = false;
    bool pinned = false;

    // Used to check that Pinned() or InGroup() have not been
    // called on the same object.
    bool pinned_called = false;
    bool in_group_called = false;

    // Sets the potential opener.
    InsertionParams& WithOpener(WebStateOpener an_opener) {
      this->opener = an_opener;
      return *this;
    }

    // Whether the WebState inherits its opener. If set, the WebState opener is
    // set to the active WebState, otherwise it must be explicitly passed via
    // `WithOpener`.
    InsertionParams& InheritOpener(bool inherits_opener = true) {
      this->inherit_opener = inherits_opener;
      return *this;
    }

    // Whether the WebState becomes the active WebState on insertion.
    InsertionParams& Activate(bool activates = true) {
      this->activate = activates;
      return *this;
    }

    // Whether the WebState is added to the pinned WebStates. This is ignored if
    // an opener is set or inherited which belongs to a group. The WebState will
    // then be inserted in that group.
    // Cannot be called after `InGroup(...)`.
    InsertionParams& Pinned(bool pin = true) {
      CHECK(!in_group_called);
      pinned_called = true;
      pinned = pin;
      return *this;
    }

    // Sets the group the new WebState belongs to.
    // Cannot be called after `Pinned(...)`.
    InsertionParams& InGroup(const TabGroup* group) {
      CHECK(!pinned_called);
      in_group_called = true;
      in_group = group;
      return *this;
    }

   private:
    // Client code should use `Automatic()` to make intention explicit.
    InsertionParams();
  };

  // TODO(crbug.com/365701685): Refactor WebStateList::ClosingFlags to use an
  // enum for the reason why a WebState is being closed.
  // Constants used when closing WebStates.
  enum ClosingFlags {
    // Used to indicate that nothing special should happen to the closed
    // WebState.
    CLOSE_NO_FLAGS = 0,

    // Used to indicate that the WebState was closed due to user action.
    CLOSE_USER_ACTION = 1 << 0,

    // Used tp indicate that the WebState was closed in a tabs clean-up.
    CLOSE_TABS_CLEANUP = 1 << 1,
  };

  // Scoped type representing a batch operation in progress.
  //
  // This is returned by `StartBatchOperation(...)`. The batch operation will
  // be considered as over when the returned instance is destroyed. To perform
  // a batch operation, the pattern is the following:
  //
  //  void DoBatchChangeOnWebStateList(WebStateList* web_state_list) {
  //    WebStateList::ScopedBatchOperation lock =
  //        web_state_list->StartBatchOperation();
  //    ... // modify the WebStateList ...
  //  }
  //
  // If the caller wants to perform a batch operation in a larger method, use
  // a scope to limit the lifetime of the ScopedBatchOperation object.
  //
  // In all case, the ScopedBatchOperation must have a smaller lifetime than
  // the WebStateList that returned it.
  //
  // Important note: the public API of WebStateList never performs an operation
  // as part of a batch operation. It is the responsibility of the calling code
  // the call StartBatchOperation() and to properly scope the returned object.
  class [[maybe_unused, nodiscard]] ScopedBatchOperation {
   public:
    ScopedBatchOperation(ScopedBatchOperation&& other)
        : web_state_list_(std::exchange(other.web_state_list_, nullptr)) {}

    ScopedBatchOperation& operator=(ScopedBatchOperation&& other) {
      web_state_list_ = std::exchange(other.web_state_list_, nullptr);
      return *this;
    }

    ~ScopedBatchOperation();

   private:
    friend class WebStateList;

    ScopedBatchOperation(WebStateList* web_state_list);

    // The WebStateList on which the batch operation is in progress.
    raw_ptr<WebStateList> web_state_list_ = nullptr;
  };

  explicit WebStateList(WebStateListDelegate* delegate);

  WebStateList(const WebStateList&) = delete;
  WebStateList& operator=(const WebStateList&) = delete;

  ~WebStateList();

  // Returns a weak pointer to the WebStateList.
  base::WeakPtr<WebStateList> AsWeakPtr();

  // Returns whether the model is empty or not.
  bool empty() const { return web_state_wrappers_.empty(); }

  // Returns the number of WebStates in the model.
  int count() const { return static_cast<int>(web_state_wrappers_.size()); }

  // Returns the number of pinned tabs. Since pinned tabs are always at the
  // beginning of the WebStateList, any tabs whose index is smaller than is
  // pinned, and any tabs whose index is greater or equal is not pinned.
  int pinned_tabs_count() const { return pinned_tabs_count_; }

  // Returns the number of regular tabs (i.e. the number of tabs that are
  // not pinned).
  int regular_tabs_count() const { return count() - pinned_tabs_count(); }

  // Returns the index of the currently active WebState, or kInvalidIndex if
  // there are no active WebState.
  int active_index() const { return active_index_; }

  // Returns true if the specified index is contained by the model.
  bool ContainsIndex(int index) const;

  // Returns true if the list is currently mutating.
  bool IsMutating() const;

  // Returns true if a batch operation is in progress.
  bool IsBatchInProgress() const;

  // Returns the currently active WebState or null if there is none.
  web::WebState* GetActiveWebState() const;

  // Returns the WebState at the specified index. It is invalid to call this
  // with an index such that `ContainsIndex(index)` returns false.
  web::WebState* GetWebStateAt(int index) const;

  // Returns the index of the specified WebState or kInvalidIndex if the
  // WebState is not in the model.
  int GetIndexOfWebState(const web::WebState* web_state) const;

  // Returns the index of the first WebState in the model whose visible URL is
  // `url` or kInvalidIndex if no WebState with that URL exists.
  int GetIndexOfWebStateWithURL(const GURL& url) const;

  // Returns the index of the first WebState, ignoring the currently active
  // WebState, in the model whose visible URL is `url` or kInvalidIndex if no
  // non-active WebState with that URL exists.
  int GetIndexOfInactiveWebStateWithURL(const GURL& url) const;

  // Returns information about the opener of the WebState at the specified
  // index. The structure `opener` will be null if there is no opener.
  WebStateOpener GetOpenerOfWebStateAt(int index) const;

  // Stores information about the opener of the WebState at the specified
  // index. The WebStateOpener `opener` must be non-null and the WebState
  // must be in WebStateList.
  void SetOpenerOfWebStateAt(int index, WebStateOpener opener);

  // Changes the pinned state of the WebState at `index`. Returns the index the
  // WebState is now at (it may have been moved to maintain contiguity of pinned
  // WebStates at the beginning of the list).
  int SetWebStatePinnedAt(int index, bool pinned);

  // Returns true if the WebState at `index` is pinned.
  bool IsWebStatePinnedAt(int index) const;

  // Inserts the specified WebState at the best position in the WebStateList
  // given `params`.
  // Returns the effective insertion index.
  int InsertWebState(std::unique_ptr<web::WebState> web_state,
                     InsertionParams params = InsertionParams::Automatic());

  // Moves the WebState at the specified index to another index.
  void MoveWebStateAt(int from_index, int to_index);

  // Replaces the WebState at the specified index with new WebState. Returns
  // the old WebState at that index to the caller (abandon ownership of the
  // returned WebState).
  std::unique_ptr<web::WebState> ReplaceWebStateAt(
      int index,
      std::unique_ptr<web::WebState> web_state);

  // Detaches the WebState at the specified index. Returns the detached WebState
  // to the caller (abandon ownership of the returned WebState).
  std::unique_ptr<web::WebState> DetachWebStateAt(int index);

  // Closes and destroys the WebState at the specified index. The `close_flags`
  // is a bitwise combination of ClosingFlags values.
  void CloseWebStateAt(int index, int close_flags);

  // Makes the WebState at the specified index the active WebState.
  void ActivateWebStateAt(int index);

  // Closes and destroys all WebStates at `removing_indexes`. The `close_flags`
  // is a bitwise combination of ClosingFlags values.
  void CloseWebStatesAtIndices(int close_flags,
                               RemovingIndexes removing_indexes);

  // Returns the tab group the WebState belongs to, if any. Otherwise, returns
  // `nullptr`.
  //
  // The returned TabGroup is valid as long as the WebStateList is not mutated.
  // To get its exact lifecycle, Listen to the group deletion notification,
  // after-which the pointer should not be used.
  const TabGroup* GetGroupOfWebStateAt(int index) const;

  // Returns the list of all groups. The order is not particularly the order in
  // which they appear in this WebStateList.
  std::set<const TabGroup*> GetGroups() const;

  // Creates a new tab group and moves the set of WebStates at `indices` to
  // it.
  // -   This unpins pinned WebState.
  // -   This ungroups grouped WebState.
  // -   This reorders the WebStates so they are contiguous and do not split an
  //     existing group in half.
  // Returns the new group.
  //
  // The returned TabGroup is valid as long as the WebStateList is not mutated.
  // To get its exact lifecycle, Listen to the group deletion notification,
  // after-which the pointer should not be used.
  const TabGroup* CreateGroup(const std::set<int>& indices,
                              const tab_groups::TabGroupVisualData& visual_data,
                              tab_groups::TabGroupId tab_group_id);

  // Returns true if the specified group is contained by the model.
  bool ContainsGroup(const TabGroup* group) const;

  // Updates the visual data for the given group.
  void UpdateGroupVisualData(const TabGroup* group,
                             const tab_groups::TabGroupVisualData& visual_data);

  // Moves the set of WebStates at `indices` at the end of the given tab group.
  void MoveToGroup(const std::set<int>& indices, const TabGroup* group);

  // Removes the set of WebStates at `indices` from the groups they are in,
  // if any. The WebStates are reordered out of the groups if necessary.
  void RemoveFromGroups(const std::set<int>& indices);

  // Moves the WebStates of `group` to be before the WebState at `before_index`.
  // To move the group at the end, pass `before_index` greater or equal to
  // `count`.
  void MoveGroup(const TabGroup* group, int before_index);

  // Removes all WebStates from the group. The WebStates stay where they are.
  // The group is destroyed.
  void DeleteGroup(const TabGroup* group);

  // Adds an observer to the model.
  void AddObserver(WebStateListObserver* observer);

  // Removes an observer from the model.
  void RemoveObserver(WebStateListObserver* observer);

  // Starts a batch operation and returns a ScopedBatchOperation. The batch
  // will be considered complete when the ScopedBatchOperation is destroyed.
  ScopedBatchOperation StartBatchOperation();

  // Invalid index.
  static constexpr int kInvalidIndex = -1;

 private:
  friend class ScopedBatchOperation;

  struct DetachParams;
  class WebStateWrapper;

  // Locks the WebStateList for mutation. This methods checks that the list is
  // not currently mutated (as the class is not re-entrant it would lead to
  // corruption of the internal state and ultimately to undefined behavior).
  base::AutoReset<bool> LockForMutation();

  // Inserts the specified WebState at the best position in the WebStateList
  // given `params`.
  // Returns the effective insertion index.
  //
  // Assumes that the WebStateList is locked.
  int InsertWebStateImpl(std::unique_ptr<web::WebState> web_state,
                         InsertionParams params);

  // Moves the WebState at the specified index to another index.
  //
  // Assumes that the WebStateList is locked.
  void MoveWebStateAtImpl(int from_index, int to_index);

  // Replaces the WebState at the specified index with new WebState. Returns
  // the old WebState at that index to the caller (abandon ownership of the
  // returned WebState).
  //
  // Assumes that the WebStateList is locked.
  std::unique_ptr<web::WebState> ReplaceWebStateAtImpl(
      int index,
      std::unique_ptr<web::WebState> web_state);

  // Detaches the WebState at the specified index. Returns the detached WebState
  // to the caller (abandon ownership of the returned WebState).
  //
  // Assumes that the WebStateList is locked.
  std::unique_ptr<web::WebState> DetachWebStateAtImpl(int index,
                                                      int new_active_index,
                                                      DetachParams params);

  // Detaches all WebStates at `removing_indexes`. Returns a vector with all the
  // detached WebStates to the caller (abandoning ownership).
  //
  // Assumes that the WebStateList is locked.
  std::vector<std::unique_ptr<web::WebState>> DetachWebStatesAtIndicesImpl(
      RemovingIndexes removing_indexes,
      DetachParams detach_params);

  // Makes the WebState at the specified index the active WebState.
  //
  // Assumes that the WebStateList is locked.
  void ActivateWebStateAtImpl(int index);

  // Changes the pinned state of the WebState at `index`. Returns the index the
  // WebState is now at (it may have been moved to maintain contiguity of pinned
  // WebStates at the beginning of the list).
  //
  // Assumes that the WebStateList is locked.
  int SetWebStatePinnedAtImpl(int index, bool pinned);

  // Creates a new tab group and moves the set of WebStates at `indices` to
  // it.
  // -   This unpins pinned WebState.
  // -   This ungroups grouped WebState.
  // -   This reorders the WebStates so they are contiguous and do not split an
  //     existing group in half.
  // Returns the new group.
  //
  // The returned TabGroup is valid as long as the WebStateList is not mutated.
  // To get its exact lifecycle, Listen to the group deletion notification,
  // after-which the pointer should not be used.
  //
  // Assumes that the WebStateList is locked.
  const TabGroup* CreateGroupImpl(
      const std::set<int>& indices,
      const tab_groups::TabGroupVisualData& visual_data,
      tab_groups::TabGroupId tab_group_id);

  // Moves the set of WebStates at `indices` at the end of the given tab group.
  //
  // Assumes that the WebStateList is locked.
  void MoveToGroupImpl(const std::set<int>& indices, const TabGroup* group);

  // Updates the visual data for the given group.
  //
  // Assumes that the WebStateList is locked.
  void UpdateGroupVisualDataImpl(
      const TabGroup* group,
      const tab_groups::TabGroupVisualData& visual_data);

  // Removes the set of WebStates at `indices` from the groups they are in,
  // if any. The WebStates are reordered out of the groups if necessary.
  //
  // Assumes that the WebStateList is locked.
  void RemoveFromGroupsImpl(const std::set<int>& indices);

  // Moves the WebStates of `group` to be before the WebState at `to_index`. To
  // move the group at the end, pass `to_index` greater or equal to `count`.
  //
  // Assumes that the WebStateList is locked.
  void MoveGroupImpl(const TabGroup* group, int to_index);

  // Removes all WebStates from the group. The WebStates stay where they are.
  // The group is destroyed.
  //
  // Assumes that the WebStateList is locked.
  void DeleteGroupImpl(const TabGroup* group);

  // Sets the opener of any WebState that reference the WebState at the
  // specified index to null.
  void ClearOpenersReferencing(int index);

  // Verifies that WebState's insertion `index` is within the proper index
  // range. `pinned` WebStates `index` should be within the pinned WebStates
  // range. Regular WebState `index` should be outside of the pinned WebStates
  // range. Returns an updated insertion `index` of the WebState.
  int ConstrainInsertionIndex(int index, bool pinned) const;

  // Verifies that WebState's move `index` is within the proper index range.
  // `pinned` WebStates `index` should be within the pinned WebStates range.
  // Regular WebState `index` should be outside of the pinned WebStates range.
  // Returns an updated move `index` of the WebState.
  int ConstrainMoveIndex(int index, bool pinned) const;

  // Returns the wrapper of the currently active WebState or null if there
  // is none.
  WebStateWrapper* GetActiveWebStateWrapper() const;

  // Returns the wrapper of the WebState at the specified index. It is invalid
  // to call this with an index such that `ContainsIndex(index)` returns false.
  WebStateWrapper* GetWebStateWrapperAt(int index) const;

  // Moves the wrapper of the WebState at `from_index` to `to_index`. This
  // performs the move, updates the relevant WebStateList state (number of
  // pinned tabs, index of the active WebState, groups ranges), and notifies
  // observers. The indices and state changes must be valid, i.e. there is no
  // fallback.
  //
  // Assumes that the WebStateList is locked.
  void MoveWebStateWrapperAt(int from_index,
                             int to_index,
                             bool pinned,
                             const TabGroup* group);

  // Removes `group` from `groups_` if `group` is empty.
  //
  // Assumes that the WebStateList is locked.
  void DeleteGroupIfEmpty(const TabGroup* group);

  // Updates the active index, updates the WebState opener for the old active
  // WebState if exists and brings the new active WebState to the "realized"
  // state.
  void SetActiveIndex(int active_index);

  // Takes action when the active WebState changes. Does nothing it
  // there is no active WebState.
  void OnActiveWebStateChanged();

  SEQUENCE_CHECKER(sequence_checker_);

  // The WebStateList delegate.
  raw_ptr<WebStateListDelegate> delegate_ = nullptr;

  // Wrappers to the WebStates hosted by the WebStateList.
  std::vector<std::unique_ptr<WebStateWrapper>> web_state_wrappers_;

  // The current set of groups.
  std::set<std::unique_ptr<TabGroup>, base::UniquePtrComparator> groups_;

  // List of observers notified of changes to the model.
  base::ObserverList<WebStateListObserver, true> observers_;

  // Index of the currently active WebState, kInvalidIndex if no such WebState.
  int active_index_ = kInvalidIndex;

  // Number of pinned tabs. Always in range from 0 to count() inclusive.
  int pinned_tabs_count_ = 0;

  // Lock to prevent observers from mutating or deleting the list while it is
  // mutating. The lock is managed by LockForMutation() method (and released
  // by the returned base::AutoReset<bool>).
  bool locked_ = false;

  // Lock to prevent nesting batched operations.
  bool batch_operation_in_progress_ = false;

  // Weak pointer factory.
  base::WeakPtrFactory<WebStateList> weak_factory_{this};
};

// Helper function that closes all WebStates in `web_state_list`. The operation
// is performed as a batch operation and thus cannot be called from another
// batch operation. The `close_flags` is a bitwise combination of ClosingFlags
// values.
void CloseAllWebStates(WebStateList& web_state_list, int close_flags);

// Helper function that closes all regular WebStates in `web_state_list`. The
// operation is performed as a batch operation and thus cannot be called from
// another batch operation. The `close_flags` is a bitwise combination of
// ClosingFlags values.
void CloseAllNonPinnedWebStates(WebStateList& web_state_list, int close_flags);

// Helper function that closes all WebStates from `group` in `web_state_list`.
// The operation is performed as a batch operation and thus cannot be called
// from another batch operation. The `close_flags` is a bitwise combination of
// ClosingFlags values.
void CloseAllWebStatesInGroup(WebStateList& web_state_list,
                              const TabGroup* group,
                              int close_flags);

// Helper function that closes all WebStates in `web_state_list` that are not at
// `index_to_keep`. The operation is performed as a batch operation and thus
// cannot be called from another batch operation. The `close_flags` is a bitwise
// combination of ClosingFlags values.
void CloseOtherWebStates(WebStateList& web_state_list,
                         int index_to_keep,
                         int close_flags);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_WEB_STATE_LIST_H_
