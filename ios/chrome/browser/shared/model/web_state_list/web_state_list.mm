// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#import <algorithm>
#import <utility>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/containers/adapters.h"
#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns whether the given flag is set in a flagset.
bool IsClosingFlagSet(int flagset, WebStateList::ClosingFlags flag) {
  return (flagset & flag) == flag;
}

}  // namespace

WebStateList::ScopedBatchOperation::ScopedBatchOperation(
    WebStateList* web_state_list)
    : web_state_list_(web_state_list) {
  DCHECK(web_state_list_);
  DCHECK(!web_state_list_->batch_operation_in_progress_);
  web_state_list_->batch_operation_in_progress_ = true;
  for (auto& observer : web_state_list_->observers_) {
    observer.WillBeginBatchOperation(web_state_list_.get());
  }
}

WebStateList::ScopedBatchOperation::~ScopedBatchOperation() {
  if (web_state_list_) {
    DCHECK(web_state_list_->batch_operation_in_progress_);
    web_state_list_->batch_operation_in_progress_ = false;
    for (auto& observer : web_state_list_->observers_) {
      observer.BatchOperationEnded(web_state_list_.get());
    }
  }
}

// Used by DetachWebStateAtImpl() and DetachWebStatesAtIndicesImpl() as
// parameter. There are 3 situations of
// 1. a WebState is detached.
// 2. a WebState is detached and closed.
// 3. a WebState is detached and closed due to an user action.
// 4. a WebState is detached and closed in a tabs clean-up.
// The static helper method helps construct a object that represents
// a valid state.
struct WebStateList::DetachParams {
  static DetachParams Detaching();
  // TODO(crbug.com/365701685): Refactor DetachParams::Closing to use an enum
  // for the reason why a WebState is being closed.
  static DetachParams Closing(bool is_user_action,
                              bool by_browsing_data_remover);

  const bool is_closing;
  const bool is_user_action;
  const bool by_browsing_data_remover;
};

WebStateList::DetachParams WebStateList::DetachParams::Detaching() {
  return {.is_closing = false, .is_user_action = false};
}

WebStateList::DetachParams WebStateList::DetachParams::Closing(
    bool is_user_action,
    bool by_browsing_data_remover) {
  return {.is_closing = true,
          .is_user_action = is_user_action,
          .by_browsing_data_remover = by_browsing_data_remover};
}

// Wrapper around a WebState stored in a WebStateList.
class WebStateList::WebStateWrapper {
 public:
  explicit WebStateWrapper(std::unique_ptr<web::WebState> web_state);

  WebStateWrapper(const WebStateWrapper&) = delete;
  WebStateWrapper& operator=(const WebStateWrapper&) = delete;

  ~WebStateWrapper();

  web::WebState* web_state() const { return web_state_.get(); }

  // Returns ownership of the wrapped WebState.
  std::unique_ptr<web::WebState> ReleaseWebState();

  // Replaces the wrapped WebState and returns the old WebState after forfeiting
  // ownership. The opener is cleared, but the group is kept.
  std::unique_ptr<web::WebState> ReplaceWebState(
      std::unique_ptr<web::WebState> web_state);

  // Gets and sets information about this WebState opener. The navigation index
  // is used to detect navigation changes during the same session.
  WebStateOpener opener() const { return opener_; }
  void SetOpener(WebStateOpener opener);

  // Gets and sets whether this WebState opener must be clear when the active
  // WebState changes.
  bool ShouldResetOpenerOnActiveWebStateChange() const;
  void SetShouldResetOpenerOnActiveWebStateChange(bool should_reset_opener);

  // Gets and sets information about this WebState group.
  const TabGroup* group() const { return group_; }
  void SetGroup(const TabGroup* group) { group_ = group; }

 private:
  std::unique_ptr<web::WebState> web_state_;
  WebStateOpener opener_;
  raw_ptr<const TabGroup> group_ = nullptr;
  bool should_reset_opener_ = false;
};

WebStateList::WebStateWrapper::WebStateWrapper(
    std::unique_ptr<web::WebState> web_state)
    : web_state_(std::move(web_state)), opener_(nullptr) {
  DCHECK(web_state_);
}

WebStateList::WebStateWrapper::~WebStateWrapper() = default;

std::unique_ptr<web::WebState>
WebStateList::WebStateWrapper::ReleaseWebState() {
  std::unique_ptr<web::WebState> web_state;
  std::swap(web_state, web_state_);
  opener_ = WebStateOpener();
  group_ = nullptr;
  return web_state;
}

std::unique_ptr<web::WebState> WebStateList::WebStateWrapper::ReplaceWebState(
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_NE(web_state.get(), web_state_.get());
  DCHECK_NE(web_state.get(), nullptr);
  std::swap(web_state, web_state_);
  opener_ = WebStateOpener();
  return web_state;
}

void WebStateList::WebStateWrapper::SetOpener(WebStateOpener opener) {
  DCHECK_NE(web_state_.get(), opener.opener);
  should_reset_opener_ = false;
  opener_ = opener;
}

bool WebStateList::WebStateWrapper::ShouldResetOpenerOnActiveWebStateChange()
    const {
  return should_reset_opener_;
}

void WebStateList::WebStateWrapper::SetShouldResetOpenerOnActiveWebStateChange(
    bool should_reset_opener) {
  should_reset_opener_ = should_reset_opener;
}

WebStateList::InsertionParams::InsertionParams() = default;

WebStateList::InsertionParams::InsertionParams(const InsertionParams& other) =
    default;

WebStateList::InsertionParams& WebStateList::InsertionParams::operator=(
    const InsertionParams& other) = default;

WebStateList::InsertionParams::InsertionParams(InsertionParams&& other) =
    default;

WebStateList::InsertionParams& WebStateList::InsertionParams::operator=(
    InsertionParams&& other) = default;

WebStateList::WebStateList(WebStateListDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

WebStateList::~WebStateList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!locked_);
  DCHECK(!batch_operation_in_progress_);

  for (auto& observer : observers_) {
    observer.WebStateListDestroyed(this);
  }

  DCHECK(!locked_);
  DCHECK(!batch_operation_in_progress_);
}

base::WeakPtr<WebStateList> WebStateList::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

bool WebStateList::ContainsIndex(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 0 <= index && index < count();
}

bool WebStateList::IsMutating() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return locked_;
}

bool WebStateList::IsBatchInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return batch_operation_in_progress_;
}

web::WebState* WebStateList::GetActiveWebState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebStateWrapper* wrapper = GetActiveWebStateWrapper();
  return wrapper ? wrapper->web_state() : nullptr;
}

web::WebState* WebStateList::GetWebStateAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetWebStateWrapperAt(index)->web_state();
}

int WebStateList::GetIndexOfWebState(const web::WebState* web_state) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state() == web_state) {
      return index;
    }
  }
  return kInvalidIndex;
}

int WebStateList::GetIndexOfWebStateWithURL(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state()->GetVisibleURL() == url) {
      return index;
    }
  }
  return kInvalidIndex;
}

int WebStateList::GetIndexOfInactiveWebStateWithURL(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (int index = 0; index < count(); ++index) {
    if (index == active_index_) {
      continue;
    }
    if (web_state_wrappers_[index]->web_state()->GetVisibleURL() == url) {
      return index;
    }
  }
  return kInvalidIndex;
}

WebStateOpener WebStateList::GetOpenerOfWebStateAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->opener();
}

void WebStateList::SetOpenerOfWebStateAt(int index, WebStateOpener opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  DCHECK(ContainsIndex(GetIndexOfWebState(opener.opener)));
  web_state_wrappers_[index]->SetOpener(opener);
}

int WebStateList::SetWebStatePinnedAt(int index, bool pinned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  auto lock = LockForMutation();
  return SetWebStatePinnedAtImpl(index, pinned);
}

bool WebStateList::IsWebStatePinnedAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  return index < pinned_tabs_count_;
}

int WebStateList::InsertWebState(std::unique_ptr<web::WebState> web_state,
                                 InsertionParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return InsertWebStateImpl(std::move(web_state), params);
}

void WebStateList::MoveWebStateAt(int from_index, int to_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return MoveWebStateAtImpl(from_index, to_index);
}

std::unique_ptr<web::WebState> WebStateList::ReplaceWebStateAt(
    int index,
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(web_state.get(), nullptr);
  auto lock = LockForMutation();
  return ReplaceWebStateAtImpl(index, std::move(web_state));
}

std::unique_ptr<web::WebState> WebStateList::DetachWebStateAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();

  OrderControllerSourceFromWebStateList source(*this);
  OrderController order_controller(source);

  const int new_active_index =
      order_controller.DetermineNewActiveIndex(active_index_, {index});

  return DetachWebStateAtImpl(index, new_active_index,
                              DetachParams::Detaching());
}

void WebStateList::CloseWebStateAt(int index, int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();

  OrderControllerSourceFromWebStateList source(*this);
  OrderController order_controller(source);

  const int new_active_index =
      order_controller.DetermineNewActiveIndex(active_index_, {index});

  const DetachParams detach_params =
      DetachParams::Closing(IsClosingFlagSet(close_flags, CLOSE_USER_ACTION),
                            IsClosingFlagSet(close_flags, CLOSE_TABS_CLEANUP));

  std::unique_ptr<web::WebState> detached_web_state =
      DetachWebStateAtImpl(index, new_active_index, detach_params);

  // Dropping detached_web_state will destroy it.
}

void WebStateList::ActivateWebStateAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index) || index == kInvalidIndex);
  auto lock = LockForMutation();
  return ActivateWebStateAtImpl(index);
}

void WebStateList::CloseWebStatesAtIndices(int close_flags,
                                           RemovingIndexes removing_indexes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();

  const DetachParams detach_params =
      DetachParams::Closing(IsClosingFlagSet(close_flags, CLOSE_USER_ACTION),
                            IsClosingFlagSet(close_flags, CLOSE_TABS_CLEANUP));

  // Detach all web states in a first pass, before destroying them at once
  // later. This avoids odd side effects as a result of WebStateImpl's
  // destructor notifying observers, including slowness during shutdown due to
  // quadratic behavior if observers iterate the WebStateList.
  std::vector<std::unique_ptr<web::WebState>> detached_web_states =
      DetachWebStatesAtIndicesImpl(removing_indexes, detach_params);

  detached_web_states.clear();
}

const TabGroup* WebStateList::GetGroupOfWebStateAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  const TabGroup* group = web_state_wrappers_[index]->group();
  DCHECK(!group || ContainsGroup(group));
  return group;
}

std::set<const TabGroup*> WebStateList::GetGroups() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<const TabGroup*> groups;
  for (const auto& group : groups_) {
    groups.insert(group.get());
  }
  return groups;
}

const TabGroup* WebStateList::CreateGroup(
    const std::set<int>& indices,
    const tab_groups::TabGroupVisualData& visual_data,
    tab_groups::TabGroupId tab_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return CreateGroupImpl(indices, visual_data, tab_group_id);
}

bool WebStateList::ContainsGroup(const TabGroup* group) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return groups_.contains(group);
}

void WebStateList::UpdateGroupVisualData(
    const TabGroup* group,
    const tab_groups::TabGroupVisualData& visual_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  UpdateGroupVisualDataImpl(group, visual_data);
}

void WebStateList::MoveToGroup(const std::set<int>& indices,
                               const TabGroup* group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  MoveToGroupImpl(indices, group);
}

void WebStateList::RemoveFromGroups(const std::set<int>& indices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  RemoveFromGroupsImpl(indices);
}

void WebStateList::MoveGroup(const TabGroup* group, int before_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  MoveGroupImpl(group, before_index);
}

void WebStateList::DeleteGroup(const TabGroup* group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  DeleteGroupImpl(group);
}

base::AutoReset<bool> WebStateList::LockForMutation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!locked_) << "WebStateList is not re-entrant; it is an error to try to "
                  << "mutate it from one of the observers (even indirectly).";

  return base::AutoReset<bool>(&locked_, /*locked=*/true);
}

int WebStateList::InsertWebStateImpl(std::unique_ptr<web::WebState> web_state,
                                     InsertionParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(web_state);
  WebStateOpener opener = params.opener;
  const bool inheriting = params.inherit_opener;
  const bool activating = params.activate;
  int index = params.desired_index;
  CHECK(!params.in_group || ContainsGroup(params.in_group));
  CHECK(!params.in_group || !params.pinned);
  const TabGroup* group = params.in_group;

  if (inheriting) {
    for (const auto& wrapper : web_state_wrappers_) {
      wrapper->SetOpener(WebStateOpener());
    }
    opener = WebStateOpener(GetActiveWebState());
  }

  // If group is not set but opener has a group, inherit the group.
  int opener_index = kInvalidIndex;
  if (opener.opener) {
    opener_index = GetIndexOfWebState(opener.opener);
    CHECK_NE(opener_index, kInvalidIndex);
    if (!group) {
      group = web_state_wrappers_[opener_index]->group();
    }
  }

  // Tab will be pinned if asked explicitly and no group could be deduced.
  const bool pinned = params.pinned && !group;

  int range_begin = 0;
  int range_end = count();
  if (group) {
    const TabGroupRange tab_group_range = group->range();
    range_begin = tab_group_range.range_begin();
    range_end = tab_group_range.range_end();
  } else if (pinned) {
    range_end = pinned_tabs_count_;
  } else {
    range_begin = pinned_tabs_count_;
  }

  const OrderController::Range range{.begin = range_begin, .end = range_end};
  const OrderControllerSourceFromWebStateList source(*this);
  const OrderController order_controller(source);
  if (index != WebStateList::kInvalidIndex) {
    index = order_controller.DetermineInsertionIndex(
        OrderController::InsertionParams::ForceIndex(index, range));
  } else if (opener.opener) {
    index = order_controller.DetermineInsertionIndex(
        OrderController::InsertionParams::WithOpener(opener_index, range));
  } else {
    index = order_controller.DetermineInsertionIndex(
        OrderController::InsertionParams::Automatic(range));
  }

  DCHECK(ContainsIndex(index) || index == count());
  delegate_->WillAddWebState(web_state.get());

  web::WebState* web_state_ptr = web_state.get();
  web_state_wrappers_.insert(
      web_state_wrappers_.begin() + index,
      std::make_unique<WebStateWrapper>(std::move(web_state)));

  if (pinned) {
    DCHECK_LE(index, pinned_tabs_count_);
    pinned_tabs_count_++;
  } else {
    DCHECK_GE(index, pinned_tabs_count_);
  }

  if (active_index_ >= index) {
    ++active_index_;
  }

  if (inheriting) {
    const auto& wrapper = web_state_wrappers_[index];
    wrapper->SetShouldResetOpenerOnActiveWebStateChange(true);
  }

  web::WebState* old_active_web_state = GetActiveWebState();
  if (activating) {
    SetActiveIndex(index);
  }

  if (opener.opener) {
    SetOpenerOfWebStateAt(index, opener);
  }

  // Ensure group contiguity: if the WebStates before and after are part of the
  // same group, honor that group instead.
  const TabGroup* prev_group =
      ContainsIndex(index - 1) ? GetGroupOfWebStateAt(index - 1) : nullptr;
  const TabGroup* next_group =
      ContainsIndex(index + 1) ? GetGroupOfWebStateAt(index + 1) : nullptr;
  if (prev_group == next_group && prev_group != nullptr) {
    group = prev_group;
  }

  // Shift groups on the right of `index` towards the right.
  web_state_wrappers_[index]->SetGroup(group);
  for (const auto& current_group : groups_) {
    TabGroupRange& current_range = current_group->range();
    if (current_group.get() == group) {
      current_range.ExpandRight();
    } else if (current_range.range_begin() >= index) {
      current_range.MoveRight();
    }
  }

  const WebStateListChangeInsert insert_change(web_state_ptr, index, group);
  const WebStateListStatus status = {
      .old_active_web_state = old_active_web_state,
      .new_active_web_state = GetActiveWebState()};
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, insert_change, status);
  }

  return index;
}

void WebStateList::MoveWebStateAtImpl(int from_index, int to_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);

  // Moves via `MoveWebStateAt` are constrained to keep their pinned state.
  const bool pinned = IsWebStatePinnedAt(from_index);
  to_index = ConstrainMoveIndex(to_index, pinned);
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));

  // Moves via `MoveWebStateAt` should update the group to ensure contiguity.
  const TabGroup* old_group = GetGroupOfWebStateAt(from_index);
  const TabGroup* new_group = nullptr;
  if (old_group && old_group->range().contains(to_index)) {
    // The move stays within the same group.
    new_group = old_group;
  } else {
    // If the WebStates before and after are part of the same group, add to that
    // group.
    const TabGroup* prev_group = nullptr;
    const TabGroup* next_group = nullptr;
    if (from_index < to_index) {
      prev_group = GetGroupOfWebStateAt(to_index);
      if (ContainsIndex(to_index + 1)) {
        next_group = GetGroupOfWebStateAt(to_index + 1);
      }
    } else {
      if (ContainsIndex(to_index - 1)) {
        prev_group = GetGroupOfWebStateAt(to_index - 1);
      }
      next_group = GetGroupOfWebStateAt(to_index);
    }
    if (prev_group && prev_group == next_group) {
      new_group = prev_group;
    }
  }

  MoveWebStateWrapperAt(from_index, to_index, pinned, new_group);
}

std::unique_ptr<web::WebState> WebStateList::ReplaceWebStateAtImpl(
    int index,
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(web_state);
  DCHECK(ContainsIndex(index));
  delegate_->WillAddWebState(web_state.get());

  ClearOpenersReferencing(index);

  web::WebState* web_state_ptr = web_state.get();
  std::unique_ptr<web::WebState> replaced_web_state =
      web_state_wrappers_[index]->ReplaceWebState(std::move(web_state));
  if (index == active_index_) {
    // The active WebState was replaced.
    OnActiveWebStateChanged();
  }

  const WebStateListChangeReplace replace_change(replaced_web_state.get(),
                                                 web_state_ptr, index);
  const WebStateListStatus status = {
      .old_active_web_state = (index == active_index_)
                                  ? replaced_web_state.get()
                                  : GetActiveWebState(),
      .new_active_web_state = GetActiveWebState()};
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, replace_change, status);
  }

  return replaced_web_state;
}

std::unique_ptr<web::WebState> WebStateList::DetachWebStateAtImpl(
    int index,
    int new_active_index,
    DetachParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(index));

  const bool is_active_web_state_detached = (index == active_index_);
  web::WebState* web_state = web_state_wrappers_[index]->web_state();
  const TabGroup* group = web_state_wrappers_[index]->group();
  const WebStateListChangeDetach detach_change(
      web_state, index, params.is_closing, params.is_user_action,
      params.by_browsing_data_remover, group);

  // `new_active_index` may be invalid e.g. when closing all the WebStates,
  // so use `ContainsIndex(...)` to avoid crashing in `GetWebStateAt(...)`.
  web::WebState* old_active_web_state = GetActiveWebState();
  web::WebState* new_active_web_state = ContainsIndex(new_active_index)
                                            ? GetWebStateAt(new_active_index)
                                            : nullptr;
  const WebStateListStatus status = {
      .old_active_web_state = old_active_web_state,
      .new_active_web_state = new_active_web_state};

  for (auto& observer : observers_) {
    observer.WebStateListWillChange(this, detach_change, status);
  }

  ClearOpenersReferencing(index);
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_wrappers_[index]->ReleaseWebState();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + index);

  // Update the number of pinned tabs if necessary.
  if (index < pinned_tabs_count_) {
    CHECK_GT(pinned_tabs_count_, 0);
    --pinned_tabs_count_;
  }

  // Update the span of the group containing the detached WebState and the
  // starting index of all groups located after the detached WebState.
  for (const auto& current_group : groups_) {
    TabGroupRange& current_range = current_group->range();
    if (current_group.get() == group) {
      current_range.ContractRight();
    } else if (current_range.range_begin() >= index) {
      current_range.MoveLeft();
    }
  }

  // Update the active index to prevent observer from seeing an invalid WebState
  // as the active one but only send the WebStateActivatedAt notification after
  // the WebStateListDidChange with kDetach.
  active_index_ = new_active_index;
  if (index < active_index_) {
    CHECK_GT(active_index_, 0);
    --active_index_;
  }

  // Check that the active element (if there is one) is valid and expected.
  DCHECK(active_index_ == kInvalidIndex || ContainsIndex(active_index_));
  DCHECK_EQ(GetActiveWebState(), new_active_web_state);

  // Inform the delegate that the active WebState changed (it may decide to
  // force its realization, ...).
  if (is_active_web_state_detached) {
    OnActiveWebStateChanged();
  }

  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, detach_change, status);
  }

  // If the group is now empty, delete it.
  DeleteGroupIfEmpty(group);

  return detached_web_state;
}

std::vector<std::unique_ptr<web::WebState>>
WebStateList::DetachWebStatesAtIndicesImpl(RemovingIndexes removing_indexes,
                                           DetachParams detach_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);

  // Immediately determine the new active index to avoid sending multiple
  // notification about changing active WebState (as they could force the
  // realization of the activated WebStates).
  OrderControllerSourceFromWebStateList source(*this);
  OrderController order_controller(source);

  int old_active_index = active_index_;
  int new_active_index =
      order_controller.DetermineNewActiveIndex(active_index_, removing_indexes);

  // Store the detached WebStates to allow the caller to delete them after
  // they have all been detached.
  std::vector<std::unique_ptr<web::WebState>> detached_web_states;

  RemovingIndexes::Range span = removing_indexes.span();
  for (int i = 0; i < span.count; ++i) {
    const int index = span.start + span.count - i - 1;
    if (!removing_indexes.Contains(index)) {
      continue;
    }

    int active_index = active_index_;
    if (index == old_active_index) {
      active_index = new_active_index;
    } else {
      if (index < new_active_index) {
        CHECK_GT(new_active_index, 0);
        --new_active_index;
      }
    }

    detached_web_states.push_back(
        DetachWebStateAtImpl(index, active_index, detach_params));
  }

  return detached_web_states;
}

void WebStateList::ActivateWebStateAtImpl(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(index) || index == kInvalidIndex);

  // Do nothing when the target WebState is already activated.
  if (active_index_ == index) {
    return;
  }

  web::WebState* old_active_web_state = GetActiveWebState();
  SetActiveIndex(index);
  web::WebState* new_active_web_state = GetActiveWebState();

  const TabGroup* group =
      index != kInvalidIndex ? GetGroupOfWebStateAt(index) : nullptr;
  const WebStateListChangeStatusOnly status_only_change(
      new_active_web_state, index, /*pinned_state_changed=*/false, group,
      group);
  const WebStateListStatus status = {
      .old_active_web_state = old_active_web_state,
      .new_active_web_state = new_active_web_state};
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, status_only_change, status);
  }
}

int WebStateList::SetWebStatePinnedAtImpl(int index, bool pinned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  if (pinned == IsWebStatePinnedAt(index)) {
    // The pinned state is not changed, nothing to do.
    return index;
  }

  // When pinning, move to the end of the pinned tabs. When unpinning, move to
  // the end of the WebStateList. May end up identical to `index`.
  int new_index = pinned ? pinned_tabs_count_ : count() - 1;
  MoveWebStateWrapperAt(index, new_index, pinned, /*new_group=*/nullptr);

  return new_index;
}

const TabGroup* WebStateList::CreateGroupImpl(
    const std::set<int>& indices,
    const tab_groups::TabGroupVisualData& visual_data,
    tab_groups::TabGroupId tab_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(!indices.empty());

  // Figure out the pivot index.
  int pivot_index = kInvalidIndex;
  const int first_index = *indices.begin();
  CHECK(ContainsIndex(first_index), base::NotFatalUntil::M128);
  if (IsWebStatePinnedAt(first_index)) {
    // Move to the last pinned tab.
    pivot_index = pinned_tabs_count_;
  } else {
    const TabGroup* group = GetGroupOfWebStateAt(first_index);
    const TabGroup* group_before = ContainsIndex(first_index - 1)
                                       ? GetGroupOfWebStateAt(first_index - 1)
                                       : nullptr;
    if (group && group == group_before) {
      pivot_index = group->range().range_end();
    } else {
      pivot_index = first_index;
    }
  }
  DCHECK_NE(pivot_index, kInvalidIndex);

  // Create the group.
  auto group = std::make_unique<TabGroup>(tab_group_id, visual_data,
                                          TabGroupRange(pivot_index, 0));
  const TabGroup* new_group = group.get();
  groups_.insert(std::move(group));

  // Notify the observers of the group creation.
  // The creation didn't change the active WebState.
  web::WebState* const active_web_state = GetActiveWebState();
  const WebStateListStatus status = {.old_active_web_state = active_web_state,
                                     .new_active_web_state = active_web_state};
  const WebStateListChangeGroupCreate group_create_change(new_group);
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, group_create_change, status);
  }

  // Move the WebStates to the group.
  MoveToGroupImpl(indices, new_group);

  return new_group;
}

void WebStateList::UpdateGroupVisualDataImpl(
    const TabGroup* group,
    const tab_groups::TabGroupVisualData& visual_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(group);
  DCHECK(ContainsGroup(group));

  // `TabGroupVisualData` comparison does not account for `is_collapsed()` but
  // this API should allow modifying `is_collapsed()` and notify observers
  // accordingly.
  if (group->visual_data() == visual_data &&
      group->visual_data().is_collapsed() == visual_data.is_collapsed()) {
    return;
  }

  // Update the visual data on the group. Find it in `groups_`, to get a
  // non-const pointer.
  const auto old_visual_data = group->visual_data();
  groups_.find(group)->get()->SetVisualData(visual_data);

  // Notify the observers.
  // The update didn't change the active WebState.
  web::WebState* const active_web_state = GetActiveWebState();
  const WebStateListStatus status = {.old_active_web_state = active_web_state,
                                     .new_active_web_state = active_web_state};
  const WebStateListChangeGroupVisualDataUpdate group_visual_data_update_change(
      group, old_visual_data);
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, group_visual_data_update_change,
                                   status);
  }
}

void WebStateList::MoveToGroupImpl(const std::set<int>& indices,
                                   const TabGroup* group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(group);
  DCHECK(ContainsGroup(group));
  DCHECK(!indices.empty());

  const TabGroupRange group_range = group->range();

  // Split indices between WebStates left of the group moving to their right and
  // WebStates right of the group moving to their left. This is to keep indices
  // valid during the moves.
  std::vector<int> before_group;
  std::vector<int> after_group;
  for (const auto& index : indices) {
    if (index < group_range.range_begin()) {
      before_group.push_back(index);
    } else if (index >= group_range.range_end()) {
      after_group.push_back(index);
    } else {
      // Indices already in the group range are not updated.
    }
  }

  // Iterate over the WebStates on the left of the group.
  // Reverse `before_group` to start from the rightmost, to keep indices valid.
  std::reverse(before_group.begin(), before_group.end());
  int to_index = group_range.range_end() - 1;
  for (int index : before_group) {
    MoveWebStateWrapperAt(index, to_index, /*pinned=*/false, group);
    --to_index;
  }

  // Iterate over the WebStates on the right of the group.
  to_index = group_range.range_end();
  for (int index : after_group) {
    MoveWebStateWrapperAt(index, to_index, /*pinned=*/false, group);
    ++to_index;
  }
}

void WebStateList::RemoveFromGroupsImpl(const std::set<int>& indices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);

  // Ungrouped WebStates are moved after the group. Iterate from the end to
  // keep ungrouped WebStates in the order they were in the group.
  for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
    const int index = *it;
    CHECK(ContainsIndex(index), base::NotFatalUntil::M128);
    const TabGroup* group = GetGroupOfWebStateAt(index);
    if (group) {
      const int to_index = group->range().range_end() - 1;
      MoveWebStateWrapperAt(index, to_index, /*pinned=*/false,
                            /*new_group=*/nullptr);
    }
  }
}

void WebStateList::DeleteGroupImpl(const TabGroup* group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(group);

  for (int index : group->range()) {
    MoveWebStateWrapperAt(index, index, /*pinned=*/false,
                          /*new_group=*/nullptr);
  }
}

void WebStateList::MoveGroupImpl(const TabGroup* group, int before_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(group);

  // Groups can't move to the pinned tabs section. Keep `count` in, to be able
  // to move a group at the end.
  int to_index = std::clamp(before_index, pinned_tabs_count_, count());

  // Destination can't be in the middle of a group. Update `to_index` to the
  // beginning of the group at the destination, if any.
  const TabGroup* group_at_destination =
      ContainsIndex(to_index) ? GetGroupOfWebStateAt(to_index) : nullptr;
  if (group_at_destination) {
    to_index = group_at_destination->range().range_begin();
  }

  // Compute the stride, i.e. by how much to change the start of the moving
  // group's range.
  const TabGroupRange prior_range = group->range();
  const int from_index = prior_range.range_begin();
  int stride = to_index - from_index;
  if (stride > 0) {
    // When moving to the right, account for the length of the moving group.
    CHECK_GE(stride, prior_range.count());
    stride -= prior_range.count();
  }

  // Early return if the group doesn't need to move.
  if (stride == 0) {
    return;
  }

  // Update the groups ranges.
  const int group_count = prior_range.count();
  for (auto& some_group : groups_) {
    TabGroupRange& some_group_range = some_group->range();
    if (some_group.get() == group) {
      // Update the moved group range.
      some_group_range.Move(stride);
    } else {
      // Slide all groups after the removed group to the left.
      if (from_index < some_group_range.range_begin()) {
        some_group_range.MoveLeft(group_count);
      }
      // Slide all groups at or after the added group to the right.
      if (from_index + stride <= some_group_range.range_begin()) {
        some_group_range.MoveRight(group_count);
      }
    }
  }

  // Update the active index if needed.
  if (prior_range.contains(active_index_)) {
    active_index_ += stride;
  } else {
    if (from_index < active_index_) {
      active_index_ -= group_count;
    }
    if (from_index + stride <= active_index_) {
      active_index_ += group_count;
    }
  }

  // Move the wrappers.
  if (from_index < to_index) {
    std::rotate(web_state_wrappers_.begin() + from_index,
                web_state_wrappers_.begin() + from_index + group_count,
                web_state_wrappers_.begin() + to_index);
  } else {
    std::rotate(web_state_wrappers_.begin() + to_index,
                web_state_wrappers_.begin() + from_index,
                web_state_wrappers_.begin() + from_index + group_count);
  }

  // Notify the observers of the change.
  // The move didn't change the active WebState.
  web::WebState* const active_web_state = GetActiveWebState();
  const WebStateListStatus status = {.old_active_web_state = active_web_state,
                                     .new_active_web_state = active_web_state};
  const WebStateListChangeGroupMove group_move_change(group, prior_range,
                                                      group->range());
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, group_move_change, status);
  }
}

void WebStateList::AddObserver(WebStateListObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void WebStateList::RemoveObserver(WebStateListObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

WebStateList::ScopedBatchOperation WebStateList::StartBatchOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!batch_operation_in_progress_);
  return ScopedBatchOperation(this);
}

void WebStateList::ClearOpenersReferencing(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::WebState* old_web_state = web_state_wrappers_[index]->web_state();
  for (auto& web_state_wrapper : web_state_wrappers_) {
    if (web_state_wrapper->opener().opener == old_web_state) {
      web_state_wrapper->SetOpener(WebStateOpener());
    }
  }
}

int WebStateList::ConstrainMoveIndex(int index, bool pinned) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pinned ? std::clamp(index, 0, pinned_tabs_count_ - 1)
                : std::clamp(index, pinned_tabs_count_, count() - 1);
}

WebStateList::WebStateWrapper* WebStateList::GetActiveWebStateWrapper() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_index_ != kInvalidIndex) {
    return GetWebStateWrapperAt(active_index_);
  }
  return nullptr;
}

WebStateList::WebStateWrapper* WebStateList::GetWebStateWrapperAt(
    int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index].get();
}

void WebStateList::MoveWebStateWrapperAt(int from_index,
                                         int to_index,
                                         bool pinned,
                                         const TabGroup* new_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));
  DCHECK(!(pinned && new_group));

  // Prepare info about the move.
  const bool index_changed = from_index != to_index;
  const bool pinned_state_changed = IsWebStatePinnedAt(from_index) != pinned;
  const TabGroup* old_group = GetGroupOfWebStateAt(from_index);
  const bool group_changed = old_group != new_group;

  // Return early if nothing has changed.
  if (!index_changed && !pinned_state_changed && !group_changed) {
    return;
  }

  // Update the pinned tabs count.
  if (pinned_state_changed) {
    if (pinned) {
      CHECK_LT(pinned_tabs_count_, count());
      pinned_tabs_count_++;
    } else {
      CHECK_GT(pinned_tabs_count_, 0);
      pinned_tabs_count_--;
    }
  }

  // Update the wrapper's group tag.
  web_state_wrappers_[from_index]->SetGroup(new_group);

  // Update the groups ranges.
  for (auto& group : groups_) {
    TabGroupRange& group_range = group->range();
    // Remove the item from the old group.
    if (group.get() == old_group) {
      group_range.ContractRight();
    }
    // Slide all groups after the removed tab to the left.
    if (from_index < group_range.range_begin()) {
      group_range.MoveLeft();
    }
    // Add the item to the new group.
    if (group.get() == new_group) {
      group_range.ExpandRight();
    } else if (to_index <= group_range.range_begin()) {
      // Slide all groups at or after the added tab to the right.
      group_range.MoveRight();
    }
  }

  // Move the wrapper.
  std::unique_ptr<WebStateWrapper> web_state_wrapper =
      std::move(web_state_wrappers_[from_index]);
  web_state_wrappers_.erase(web_state_wrappers_.begin() + from_index);
  web_state_wrappers_.insert(web_state_wrappers_.begin() + to_index,
                             std::move(web_state_wrapper));

  // Update the active index if needed.
  if (active_index_ == from_index) {
    active_index_ = to_index;
  } else {
    int min = std::min(from_index, to_index);
    int max = std::max(from_index, to_index);
    int delta = from_index < to_index ? -1 : +1;
    if (min <= active_index_ && active_index_ <= max) {
      active_index_ += delta;
    }
  }

  // Notify the observers of the change.
  // The move didn't change the active WebState.
  web::WebState* const active_web_state = GetActiveWebState();
  const WebStateListStatus status = {.old_active_web_state = active_web_state,
                                     .new_active_web_state = active_web_state};
  if (index_changed) {
    const WebStateListChangeMove move_change(
        GetWebStateAt(to_index), from_index, to_index, pinned_state_changed,
        old_group, new_group);
    for (auto& observer : observers_) {
      observer.WebStateListDidChange(this, move_change, status);
    }
  } else {
    DCHECK(pinned_state_changed || group_changed);
    const WebStateListChangeStatusOnly status_only_change(
        GetWebStateAt(to_index), to_index, pinned_state_changed, old_group,
        new_group);
    for (auto& observer : observers_) {
      observer.WebStateListDidChange(this, status_only_change, status);
    }
  }

  // If the old group is now empty, delete it.
  DeleteGroupIfEmpty(old_group);
}

void WebStateList::DeleteGroupIfEmpty(const TabGroup* group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);

  const auto iter = groups_.find(group);
  if (iter != groups_.end() && group->range().count() == 0) {
    // Notify observers of the imminent deletion of the group.
    // The deletion doesn't change the active WebState.
    web::WebState* const active_web_state = GetActiveWebState();
    const WebStateListStatus status = {
        .old_active_web_state = active_web_state,
        .new_active_web_state = active_web_state};
    const WebStateListChangeGroupDelete group_delete_change(group);
    for (auto& observer : observers_) {
      observer.WebStateListDidChange(this, group_delete_change, status);
    }

    // Actually delete the group.
    groups_.erase(iter);
  }
}

void WebStateList::SetActiveIndex(int active_index) {
  if (active_index_ == active_index) {
    return;
  }

  WebStateWrapper* old_active_web_state_wrapper = GetActiveWebStateWrapper();
  if (old_active_web_state_wrapper &&
      old_active_web_state_wrapper->ShouldResetOpenerOnActiveWebStateChange()) {
    // Clear the opener when the active WebState changes.
    old_active_web_state_wrapper->SetOpener(WebStateOpener());
  }

  active_index_ = active_index;
  OnActiveWebStateChanged();
}

void WebStateList::OnActiveWebStateChanged() {
  web::WebState* active_web_state = GetActiveWebState();
  if (active_web_state) {
    delegate_->WillActivateWebState(active_web_state);
  }
}

void CloseAllWebStates(WebStateList& web_state_list, int close_flags) {
  const int count = web_state_list.count();

  const WebStateList::ScopedBatchOperation batch =
      web_state_list.StartBatchOperation();
  web_state_list.CloseWebStatesAtIndices(close_flags, RemovingIndexes({
                                                          .start = 0,
                                                          .count = count,
                                                      }));
}

void CloseAllNonPinnedWebStates(WebStateList& web_state_list, int close_flags) {
  const int pinned_tabs_count = web_state_list.pinned_tabs_count();
  const int regular_tabs_count = web_state_list.count() - pinned_tabs_count;

  const WebStateList::ScopedBatchOperation batch =
      web_state_list.StartBatchOperation();
  web_state_list.CloseWebStatesAtIndices(close_flags,
                                         RemovingIndexes({
                                             .start = pinned_tabs_count,
                                             .count = regular_tabs_count,
                                         }));
}

void CloseAllWebStatesInGroup(WebStateList& web_state_list,
                              const TabGroup* group,
                              int close_flags) {
  const TabGroupRange range = group->range();

  const WebStateList::ScopedBatchOperation batch =
      web_state_list.StartBatchOperation();
  web_state_list.CloseWebStatesAtIndices(close_flags,
                                         RemovingIndexes({
                                             .start = range.range_begin(),
                                             .count = range.count(),
                                         }));
}

void CloseOtherWebStates(WebStateList& web_state_list,
                         int index_to_keep,
                         int close_flags) {
  const int count = web_state_list.count();
  const int pinned_count = web_state_list.pinned_tabs_count();
  std::vector<int> indexes_to_close;
  indexes_to_close.reserve(count - pinned_count);
  for (int index = pinned_count; index < count; ++index) {
    if (index == index_to_keep) {
      continue;
    }
    indexes_to_close.push_back(index);
  }
  const WebStateList::ScopedBatchOperation batch =
      web_state_list.StartBatchOperation();
  web_state_list.CloseWebStatesAtIndices(
      close_flags, RemovingIndexes(std::move(indexes_to_close)));
}
