// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#import <algorithm>
#import <utility>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/containers/adapters.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
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
// The static helper method helps construct a object that represents
// a valid state.
struct WebStateList::DetachParams {
  static DetachParams Detaching();
  static DetachParams Closing(bool is_user_action);

  const bool is_closing;
  const bool is_user_action;
};

WebStateList::DetachParams WebStateList::DetachParams::Detaching() {
  return {.is_closing = false, .is_user_action = false};
}

WebStateList::DetachParams WebStateList::DetachParams::Closing(
    bool is_user_action) {
  return {.is_closing = true, .is_user_action = is_user_action};
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

  // Replaces the wrapped WebState (and clear associated state) and returns the
  // old WebState after forfeiting ownership.
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

 private:
  std::unique_ptr<web::WebState> web_state_;
  WebStateOpener opener_;
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
  to_index = ConstrainMoveIndex(to_index, IsWebStatePinnedAt(from_index));
  return MoveWebStateAtImpl(from_index, to_index,
                            /*pinned_state_change=*/false);
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
      DetachParams::Closing(IsClosingFlagSet(close_flags, CLOSE_USER_ACTION));

  std::unique_ptr<web::WebState> detached_web_state =
      DetachWebStateAtImpl(index, new_active_index, detach_params);

  // Dropping detached_web_state will destroy it.
}

void WebStateList::CloseAllWebStates(int close_flags) {
  CloseWebStatesAtIndices(close_flags, RemovingIndexes({
                                           .start = 0,
                                           .count = count(),
                                       }));
}

void WebStateList::CloseAllNonPinnedWebStates(int close_flags) {
  CloseWebStatesAtIndices(close_flags,
                          RemovingIndexes({
                              .start = pinned_tabs_count_,
                              .count = count() - pinned_tabs_count_,
                          }));
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

  ScopedBatchOperation batch = StartBatchOperation();
  const DetachParams detach_params =
      DetachParams::Closing(IsClosingFlagSet(close_flags, CLOSE_USER_ACTION));

  // Detach all web states in a first pass, before destroying them at once
  // later. This avoids odd side effects as a result of WebStateImpl's
  // destructor notifying observers, including slowness during shutdown due to
  // quadratic behavior if observers iterate the WebStateList.
  std::vector<std::unique_ptr<web::WebState>> detached_web_states =
      DetachWebStatesAtIndicesImpl(removing_indexes, detach_params);
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
  const bool pinned = params.pinned;
  int index = params.desired_index;

  if (inheriting) {
    for (const auto& wrapper : web_state_wrappers_) {
      wrapper->SetOpener(WebStateOpener());
    }
    opener = WebStateOpener(GetActiveWebState());
  }

  const OrderController::Range range{
      .begin = pinned ? 0 : pinned_tabs_count_,
      .end = pinned ? pinned_tabs_count_ : count(),
  };

  const OrderControllerSourceFromWebStateList source(*this);
  const OrderController order_controller(source);
  if (index != WebStateList::kInvalidIndex) {
    index = order_controller.DetermineInsertionIndex(
        OrderController::InsertionParams::ForceIndex(index, range));
  } else if (opener.opener) {
    const int opener_index = GetIndexOfWebState(opener.opener);
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

  const WebStateListChangeInsert insert_change(web_state_ptr);
  const WebStateListStatus status = {
      .index = index,
      .pinned_state_change = false,
      .old_active_web_state = old_active_web_state,
      .new_active_web_state = GetActiveWebState()};
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, insert_change, status);
  }

  return index;
}

void WebStateList::MoveWebStateAtImpl(int from_index,
                                      int to_index,
                                      bool pinned_state_change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));

  if (from_index == to_index) {
    if (pinned_state_change) {
      // Notify the event to the observers that the pinned state is updated but
      // the layout in WebStateList isn't updated.
      const WebStateListChangeStatusOnly status_only_change(
          web_state_wrappers_[to_index]->web_state());
      const WebStateListStatus status = {
          .index = to_index,
          .pinned_state_change = true,
          // An active WebState doesn't change when a pinned state is updated.
          .old_active_web_state = GetActiveWebState(),
          .new_active_web_state = GetActiveWebState()};
      for (auto& observer : observers_) {
        observer.WebStateListDidChange(this, status_only_change, status);
      }
    }
    return;
  }

  std::unique_ptr<WebStateWrapper> web_state_wrapper =
      std::move(web_state_wrappers_[from_index]);
  web::WebState* web_state = web_state_wrapper->web_state();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + from_index);
  web_state_wrappers_.insert(web_state_wrappers_.begin() + to_index,
                             std::move(web_state_wrapper));

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

  const WebStateListChangeMove move_change(web_state, from_index);
  const WebStateListStatus status = {
      .index = to_index,
      .pinned_state_change = pinned_state_change,
      // The move operation doesn't insert/delete a WebState and doesn't change
      // an active WebState.
      .old_active_web_state = GetActiveWebState(),
      .new_active_web_state = GetActiveWebState()};
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, move_change, status);
  }
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
                                                 web_state_ptr);
  const WebStateListStatus status = {
      .index = index,
      .pinned_state_change = false,
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
  const WebStateListChangeDetach detach_change(web_state, params.is_closing,
                                               params.is_user_action);

  // `new_active_index` may be invalid e.g. when closing all the WebStates,
  // so use `ContainsIndex(...)` to avoid crashing in `GetWebStateAt(...)`.
  web::WebState* old_active_web_state = GetActiveWebState();
  web::WebState* new_active_web_state = ContainsIndex(new_active_index)
                                            ? GetWebStateAt(new_active_index)
                                            : nullptr;

  {
    const WebStateListStatus status = {
        .index = index,
        .pinned_state_change = false,
        .old_active_web_state = old_active_web_state,
        .new_active_web_state = new_active_web_state};
    for (auto& observer : observers_) {
      observer.WebStateListWillChange(this, detach_change, status);
    }
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

  const WebStateListStatus status = {
      .index = index,
      .pinned_state_change = false,
      .old_active_web_state = old_active_web_state,
      .new_active_web_state = new_active_web_state};
  for (auto& observer : observers_) {
    observer.WebStateListDidChange(this, detach_change, status);
  }

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

  const WebStateListChangeStatusOnly status_only_change(old_active_web_state);
  const WebStateListStatus status = {
      .index = index,
      .pinned_state_change = false,
      .old_active_web_state = old_active_web_state,
      .new_active_web_state = GetActiveWebState()};
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

  int new_index = index;
  if (pinned) {
    // Move the tab to the end of the pinned tabs. May be a no-op.
    CHECK_LT(pinned_tabs_count_, count());
    new_index = pinned_tabs_count_;
    pinned_tabs_count_++;
  } else {
    // Move the tab to the end of the WebStateList. May be a no-op.
    CHECK_GT(pinned_tabs_count_, 0);
    new_index = count() - 1;
    pinned_tabs_count_--;
  }

  // The pinned state update is notified in `MoveWebStateAtImpl()` with the type
  // of `kMove` when a WebState is moved or `kStatusOnly` when a WebState is
  // not moved.
  MoveWebStateAtImpl(index, new_index, /*pinned_state_change=*/true);

  return new_index;
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

// static
const int WebStateList::kInvalidIndex;
