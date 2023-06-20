// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#import <algorithm>
#import <utility>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/containers/adapters.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns whether the given flag is set in a flagset.
bool IsInsertionFlagSet(int flagset, WebStateList::InsertionFlags flag) {
  return (flagset & flag) == flag;
}

// Returns whether the given flag is set in a flagset.
bool IsClosingFlagSet(int flagset, WebStateList::ClosingFlags flag) {
  return (flagset & flag) == flag;
}

}  // namespace

// Wrapper around a WebState stored in a WebStateList.
class WebStateList::WebStateWrapper {
 public:
  WebStateWrapper(std::unique_ptr<web::WebState> web_state, bool pinned);

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

  // Gets and sets information about this WebState being pinned.
  bool pinned() const { return pinned_; }
  void set_pinned(bool pinned) { pinned_ = pinned; }

  // Gets and sets whether this WebState opener must be clear when the active
  // WebState changes.
  bool ShouldResetOpenerOnActiveWebStateChange() const;
  void SetShouldResetOpenerOnActiveWebStateChange(bool should_reset_opener);

  // Returns whether `opener` spawned the wrapped WebState. If `use_group` is
  // true, also use the opener navigation index to detect navigation changes
  // during the same session.
  bool WasOpenedBy(const web::WebState* opener,
                   int opener_navigation_index,
                   bool use_group) const;

 private:
  std::unique_ptr<web::WebState> web_state_;
  WebStateOpener opener_;
  bool should_reset_opener_ = false;
  bool pinned_ = false;
};

WebStateList::WebStateWrapper::WebStateWrapper(
    std::unique_ptr<web::WebState> web_state,
    bool pinned)
    : web_state_(std::move(web_state)), opener_(nullptr), pinned_(pinned) {
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

bool WebStateList::WebStateWrapper::WasOpenedBy(const web::WebState* opener,
                                                int opener_navigation_index,
                                                bool use_group) const {
  DCHECK(opener);
  if (opener_.opener != opener) {
    return false;
  }

  if (!use_group) {
    return true;
  }

  return opener_.navigation_index == opener_navigation_index;
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
  CloseAllWebStates(CLOSE_NO_FLAGS);
  for (auto& observer : observers_) {
    observer.WebStateListDestroyed(this);
  }
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

int WebStateList::GetIndexOfNextWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, 1);
}

int WebStateList::GetIndexOfLastWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, INT_MAX);
}

int WebStateList::SetWebStatePinnedAt(int index, bool pinned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  auto lock = LockForMutation();
  return SetWebStatePinnedImpl(index, pinned);
}

bool WebStateList::IsWebStatePinnedAt(int index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->pinned();
}

int WebStateList::InsertWebState(int index,
                                 std::unique_ptr<web::WebState> web_state,
                                 int insertion_flags,
                                 WebStateOpener opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return InsertWebStateImpl(index, std::move(web_state), insertion_flags,
                            opener);
}

void WebStateList::MoveWebStateAt(int from_index, int to_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  to_index = ConstrainMoveIndex(to_index, IsWebStatePinnedAt(from_index));
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
  return DetachWebStateAtImpl(index);
}

void WebStateList::CloseWebStateAt(int index, int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  return CloseWebStateAtImpl(index, close_flags);
}

void WebStateList::CloseAllWebStates(int close_flags) {
  CloseAllWebStatesAfterIndex(0, close_flags);
}

void WebStateList::CloseAllNonPinnedWebStates(int close_flags) {
  CloseAllWebStatesAfterIndex(GetIndexOfFirstNonPinnedWebState(), close_flags);
}

void WebStateList::ActivateWebStateAt(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsIndex(index));
  auto lock = LockForMutation();
  return ActivateWebStateAtImpl(index, ActiveWebStateChangeReason::Activated);
}

base::AutoReset<bool> WebStateList::LockForMutation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!locked_) << "WebStateList is not re-entrant; it is an error to try to "
                  << "mutate it from one of the observers (even indirectly).";

  return base::AutoReset<bool>(&locked_, /*locked=*/true);
}

int WebStateList::InsertWebStateImpl(int index,
                                     std::unique_ptr<web::WebState> web_state,
                                     int insertion_flags,
                                     WebStateOpener opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(web_state);
  const bool activating = IsInsertionFlagSet(insertion_flags, INSERT_ACTIVATE);
  const bool forced = IsInsertionFlagSet(insertion_flags, INSERT_FORCE_INDEX);
  const bool pinned = IsInsertionFlagSet(insertion_flags, INSERT_PINNED);

  if (IsInsertionFlagSet(insertion_flags, INSERT_INHERIT_OPENER)) {
    for (const auto& wrapper : web_state_wrappers_) {
      wrapper->SetOpener(WebStateOpener());
    }
    opener = WebStateOpener(GetActiveWebState());
  }

  WebStateListOrderController order_controller(*this);
  index = order_controller.DetermineInsertionIndex(index, opener.opener, forced,
                                                   pinned);

  DCHECK(ContainsIndex(index) || index == count());
  delegate_->WillAddWebState(web_state.get());

  web::WebState* web_state_ptr = web_state.get();
  web_state_wrappers_.insert(
      web_state_wrappers_.begin() + index,
      std::make_unique<WebStateWrapper>(std::move(web_state), pinned));

  if (active_index_ >= index) {
    ++active_index_;
  }

  if (IsInsertionFlagSet(insertion_flags, INSERT_INHERIT_OPENER)) {
    const auto& wrapper = web_state_wrappers_[index];
    wrapper->SetShouldResetOpenerOnActiveWebStateChange(true);
  }

  const WebStateListChangeInsert insert_change(web_state_ptr);
  const WebStateSelection selection = {.index = index,
                                       .activating = activating};
  for (auto& observer : observers_) {
    observer.WebStateListChanged(this, insert_change, selection);
  }

  if (opener.opener) {
    SetOpenerOfWebStateAt(index, opener);
  }

  if (activating) {
    ActivateWebStateAtImpl(index, ActiveWebStateChangeReason::Inserted);
  }

  return index;
}

void WebStateList::MoveWebStateAtImpl(int from_index, int to_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));

  if (from_index == to_index) {
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
  const WebStateSelection selection = {.index = to_index, .activating = false};
  for (auto& observer : observers_) {
    observer.WebStateListChanged(this, move_change, selection);
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

  const WebStateListChangeReplace replace_change(replaced_web_state.get(),
                                                 web_state_ptr);
  const WebStateSelection selection = {.index = index,
                                       .activating = (index == active_index_)};
  for (auto& observer : observers_) {
    observer.WebStateListChanged(this, replace_change, selection);
  }

  // When the active WebState is replaced, notify the observers as nearly
  // all of them needs to treat a replacement as the selection changed.
  NotifyIfActiveWebStateChanged(replaced_web_state.get(),
                                ActiveWebStateChangeReason::Replaced);

  delegate_->WebStateDetached(replaced_web_state.get());
  return replaced_web_state;
}

std::unique_ptr<web::WebState> WebStateList::DetachWebStateAtImpl(int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(index));
  web::WebState* web_state = web_state_wrappers_[index]->web_state();
  for (auto& observer : observers_) {
    observer.WillDetachWebStateAt(this, web_state, index);
  }

  // Update the active index to prevent observer from seeing an invalid WebState
  // as the active one but only send the WebStateActivatedAt notification after
  // the WebStateDetachedAt one.
  WebStateListOrderController order_controller(*this);
  const bool active_web_state_was_closed = (index == active_index_);
  active_index_ =
      order_controller.DetermineNewActiveIndex(active_index_, {index});

  ClearOpenersReferencing(index);
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_wrappers_[index]->ReleaseWebState();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + index);

  // Check that the active element (if there is one) is valid.
  DCHECK(active_index_ == kInvalidIndex || ContainsIndex(active_index_));

  const WebStateListChangeDetach detach_change(web_state);
  // TODO(crbug.com/1442546): Remove `activating` and introduce `active_index`
  // which is the index of the currently active WebState.
  const WebStateSelection selection = {.index = index, .activating = false};
  for (auto& observer : observers_) {
    observer.WebStateListChanged(this, detach_change, selection);
  }

  if (active_web_state_was_closed) {
    NotifyIfActiveWebStateChanged(web_state,
                                  ActiveWebStateChangeReason::Closed);
  }

  delegate_->WebStateDetached(web_state);
  return detached_web_state;
}

void WebStateList::CloseWebStateAtImpl(int index, int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  std::unique_ptr<web::WebState> detached_web_state =
      DetachWebStateAtImpl(index);
  const bool user_action = IsClosingFlagSet(close_flags, CLOSE_USER_ACTION);
  for (auto& observer : observers_) {
    observer.WillCloseWebStateAt(this, detached_web_state.get(), index,
                                 user_action);
  }

  // Dropping detached_web_state will destroy it.
}

void WebStateList::CloseAllWebStatesAfterIndex(int start_index,
                                               int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto lock = LockForMutation();
  PerformBatchOperation(base::BindOnce(
      [](int start_index, int close_flags, WebStateList* web_state_list) {
        web_state_list->CloseAllWebStatesAfterIndexImpl(start_index,
                                                        close_flags);
      },
      start_index, close_flags));
}

void WebStateList::CloseAllWebStatesAfterIndexImpl(int start_index,
                                                   int close_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);

  // Immediately determine the new active index to avoid
  // sending multiple notification about changing active
  // WebState.
  int new_active_index = kInvalidIndex;
  if (start_index != 0) {
    std::vector<int> removing_indexes;
    removing_indexes.reserve(count() - start_index);
    for (int i = start_index; i < count(); ++i) {
      removing_indexes.push_back(i);
    }

    WebStateListOrderController order_controller(*this);
    new_active_index = order_controller.DetermineNewActiveIndex(
        active_index_,
        WebStateListRemovingIndexes(std::move(removing_indexes)));
  }

  ActivateWebStateAtImpl(new_active_index, ActiveWebStateChangeReason::Closed);

  while (count() > start_index) {
    CloseWebStateAtImpl(count() - 1, close_flags);
  }
}

void WebStateList::ActivateWebStateAtImpl(int index,
                                          ActiveWebStateChangeReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(locked_);
  DCHECK(ContainsIndex(index) || index == kInvalidIndex);
  WebStateWrapper* old_web_state_wrapper = GetActiveWebStateWrapper();
  if (old_web_state_wrapper) {
    if (old_web_state_wrapper->ShouldResetOpenerOnActiveWebStateChange()) {
      old_web_state_wrapper->SetOpener(WebStateOpener());
    }
  }

  active_index_ = index;
  NotifyIfActiveWebStateChanged(
      old_web_state_wrapper ? old_web_state_wrapper->web_state() : nullptr,
      reason);
}

void WebStateList::AddObserver(WebStateListObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void WebStateList::RemoveObserver(WebStateListObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void WebStateList::PerformBatchOperation(
    base::OnceCallback<void(WebStateList*)> operation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!batch_operation_in_progress_);
  base::AutoReset<bool> lock(&batch_operation_in_progress_, /*locked=*/true);

  for (auto& observer : observers_) {
    observer.WillBeginBatchOperation(this);
  }
  if (!operation.is_null()) {
    std::move(operation).Run(this);
  }
  for (auto& observer : observers_) {
    observer.BatchOperationEnded(this);
  }
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

void WebStateList::NotifyIfActiveWebStateChanged(
    web::WebState* old_web_state,
    ActiveWebStateChangeReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::WebState* new_web_state = GetActiveWebState();
  if (old_web_state == new_web_state) {
    return;
  }

  if (new_web_state) {
    // Do not trigger a CheckForOverRealization here, as it's expected
    // that many WebStates may realize actions like side swipe or quickly
    // multiple tabs.
    web::IgnoreOverRealizationCheck();
    new_web_state->ForceRealized();
  }

  for (auto& observer : observers_) {
    observer.WebStateActivatedAt(this, old_web_state, new_web_state,
                                 active_index_, reason);
  }
}

int WebStateList::GetIndexOfNthWebStateOpenedBy(const web::WebState* opener,
                                                int start_index,
                                                bool use_group,
                                                int n) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(n, 0);
  if (!opener || !ContainsIndex(start_index)) {
    return kInvalidIndex;
  }

  const int opener_navigation_index =
      use_group ? opener->GetNavigationManager()->GetLastCommittedItemIndex()
                : -1;

  int found_index = kInvalidIndex;
  const int list_length = count();
  for (int i = 1; i < list_length; ++i) {
    const int index = (start_index + i) % list_length;
    DCHECK_NE(index, start_index);

    const auto& wrapper = web_state_wrappers_[index];
    if (!wrapper->WasOpenedBy(opener, opener_navigation_index, use_group)) {
      continue;
    }

    found_index = index;
    if (--n == 0) {
      break;
    }
  }

  return found_index;
}

int WebStateList::SetWebStatePinnedImpl(int index, bool pinned) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_state_wrappers_[index]->pinned() == pinned) {
    return index;
  }

  // The tab's position may have to change as the pinned tab state is changing.
  int non_pinned_web_state_index = GetIndexOfFirstNonPinnedWebState();
  web_state_wrappers_[index]->set_pinned(pinned);

  if (pinned && index != non_pinned_web_state_index) {
    MoveWebStateAtImpl(index, non_pinned_web_state_index);
    index = non_pinned_web_state_index;
  } else if (!pinned) {
    // Unpinned WebStates should be moved to the end of the list.
    MoveWebStateAtImpl(index, count() - 1);
    index = count() - 1;
  }

  for (auto& observer : observers_) {
    observer.WebStatePinnedStateChanged(
        this, web_state_wrappers_[index]->web_state(), index);
  }

  return index;
}

int WebStateList::GetIndexOfFirstNonPinnedWebState() const {
  for (size_t index = 0; index < web_state_wrappers_.size(); ++index) {
    if (!web_state_wrappers_[index]->pinned()) {
      return static_cast<int>(index);
    }
  }
  // There are only pinned WebStates.
  return count();
}

int WebStateList::ConstrainMoveIndex(int index, bool pinned) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pinned ? std::clamp(index, 0, GetIndexOfFirstNonPinnedWebState() - 1)
                : std::clamp(index, GetIndexOfFirstNonPinnedWebState(),
                             count() - 1);
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

// static
const int WebStateList::kInvalidIndex;
