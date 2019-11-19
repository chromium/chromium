// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
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
  explicit WebStateWrapper(std::unique_ptr<web::WebState> web_state);
  ~WebStateWrapper();

  web::WebState* web_state() const { return web_state_.get(); }

  // Replaces the wrapped WebState (and clear associated state) and returns the
  // old WebState after forfeiting ownership.
  std::unique_ptr<web::WebState> ReplaceWebState(
      std::unique_ptr<web::WebState> web_state);

  // Gets and sets information about this WebState opener. The navigation index
  // is used to detect navigation changes during the same session.
  WebStateOpener opener() const { return opener_; }
  void set_opener(WebStateOpener opener) { opener_ = opener; }

  // Returns whether |opener| spawned the wrapped WebState. If |use_group| is
  // true, also use the opener navigation index to detect navigation changes
  // during the same session.
  bool WasOpenedBy(const web::WebState* opener,
                   int opener_navigation_index,
                   bool use_group) const;

 private:
  std::unique_ptr<web::WebState> web_state_;
  WebStateOpener opener_;

  DISALLOW_COPY_AND_ASSIGN(WebStateWrapper);
};

WebStateList::WebStateWrapper::WebStateWrapper(
    std::unique_ptr<web::WebState> web_state)
    : web_state_(std::move(web_state)), opener_(nullptr) {
  DCHECK(web_state_);
}

WebStateList::WebStateWrapper::~WebStateWrapper() = default;

std::unique_ptr<web::WebState> WebStateList::WebStateWrapper::ReplaceWebState(
    std::unique_ptr<web::WebState> web_state) {
  DCHECK_NE(web_state.get(), web_state_.get());
  std::swap(web_state, web_state_);
  opener_ = WebStateOpener();
  return web_state;
}

bool WebStateList::WebStateWrapper::WasOpenedBy(const web::WebState* opener,
                                                int opener_navigation_index,
                                                bool use_group) const {
  DCHECK(opener);
  if (opener_.opener != opener)
    return false;

  if (!use_group)
    return true;

  return opener_.navigation_index == opener_navigation_index;
}

WebStateList::WebStateList(WebStateListDelegate* delegate)
    : delegate_(delegate),
      order_controller_(std::make_unique<WebStateListOrderController>(this)) {
  DCHECK(delegate_);
}

WebStateList::~WebStateList() {
  CHECK(!locked_);
  CloseAllWebStates(CLOSE_NO_FLAGS);
}

bool WebStateList::ContainsIndex(int index) const {
  return 0 <= index && index < count();
}

bool WebStateList::IsMutating() const {
  return locked_;
}

web::WebState* WebStateList::GetActiveWebState() const {
  if (active_index_ != kInvalidIndex)
    return GetWebStateAt(active_index_);
  return nullptr;
}

web::WebState* WebStateList::GetWebStateAt(int index) const {
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->web_state();
}

int WebStateList::GetIndexOfWebState(const web::WebState* web_state) const {
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state() == web_state)
      return index;
  }
  return kInvalidIndex;
}

int WebStateList::GetIndexOfWebStateWithURL(const GURL& url) const {
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state()->GetVisibleURL() == url)
      return index;
  }
  return kInvalidIndex;
}

int WebStateList::GetIndexOfInactiveWebStateWithURL(const GURL& url) const {
  for (int index = 0; index < count(); ++index) {
    if (index == active_index_)
      continue;
    if (web_state_wrappers_[index]->web_state()->GetVisibleURL() == url)
      return index;
  }
  return kInvalidIndex;
}

WebStateOpener WebStateList::GetOpenerOfWebStateAt(int index) const {
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->opener();
}

void WebStateList::SetOpenerOfWebStateAt(int index, WebStateOpener opener) {
  DCHECK(ContainsIndex(index));
  DCHECK(ContainsIndex(GetIndexOfWebState(opener.opener)));
  web_state_wrappers_[index]->set_opener(opener);
}

int WebStateList::GetIndexOfNextWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, 1);
}

int WebStateList::GetIndexOfLastWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, INT_MAX);
}

int WebStateList::InsertWebState(int index,
                                 std::unique_ptr<web::WebState> web_state,
                                 int insertion_flags,
                                 WebStateOpener opener) {
  const bool activating = IsInsertionFlagSet(insertion_flags, INSERT_ACTIVATE);

  {
    // Inner block for the mutation lock, because ActivateWebState might need to
    // be called (if |activating| is true), and that method has its own mutation
    // lock.
    CHECK(!locked_);
    base::AutoReset<bool> scoped_lock(&locked_, /* locked */ true);
    if (IsInsertionFlagSet(insertion_flags, INSERT_INHERIT_OPENER))
      opener = WebStateOpener(GetActiveWebState());

    if (!IsInsertionFlagSet(insertion_flags, INSERT_FORCE_INDEX)) {
      index = order_controller_->DetermineInsertionIndex(opener.opener);
      if (index < 0 || count() < index)
        index = count();
    }

    DCHECK(ContainsIndex(index) || index == count());
    delegate_->WillAddWebState(web_state.get());

    web::WebState* web_state_ptr = web_state.get();
    web_state_wrappers_.insert(
        web_state_wrappers_.begin() + index,
        std::make_unique<WebStateWrapper>(std::move(web_state)));

    if (active_index_ >= index)
      ++active_index_;

    for (auto& observer : observers_)
      observer.WebStateInsertedAt(this, web_state_ptr, index, activating);

    if (opener.opener)
      SetOpenerOfWebStateAt(index, opener);
  }

  if (activating)
    ActivateWebStateAt(index);

  return index;
}

void WebStateList::MoveWebStateAt(int from_index, int to_index) {
  CHECK(!locked_);
  base::AutoReset<bool> scoped_lock(&locked_, /* locked */ true);
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));
  if (from_index == to_index)
    return;

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
    if (min <= active_index_ && active_index_ <= max)
      active_index_ += delta;
  }

  for (auto& observer : observers_)
    observer.WebStateMoved(this, web_state, from_index, to_index);
}

std::unique_ptr<web::WebState> WebStateList::ReplaceWebStateAt(
    int index,
    std::unique_ptr<web::WebState> web_state) {
  DCHECK(ContainsIndex(index));
  delegate_->WillAddWebState(web_state.get());

  ClearOpenersReferencing(index);

  web::WebState* web_state_ptr = web_state.get();
  std::unique_ptr<web::WebState> old_web_state =
      web_state_wrappers_[index]->ReplaceWebState(std::move(web_state));

  for (auto& observer : observers_) {
    observer.WebStateReplacedAt(this, old_web_state.get(), web_state_ptr,
                                index);
  }

  // When the active WebState is replaced, notify the observers as nearly
  // all of them needs to treat a replacement as the selection changed.
  NotifyIfActiveWebStateChanged(old_web_state.get(),
                                WebStateListObserver::CHANGE_REASON_REPLACED);

  delegate_->WebStateDetached(old_web_state.get());
  return old_web_state;
}

std::unique_ptr<web::WebState> WebStateList::DetachWebStateAt(int index) {
  CHECK(!locked_);
  base::AutoReset<bool> scoped_lock(&locked_, /* locked */ true);
  DCHECK(ContainsIndex(index));
  int new_active_index = order_controller_->DetermineNewActiveIndex(index);

  web::WebState* web_state = web_state_wrappers_[index]->web_state();
  for (auto& observer : observers_)
    observer.WillDetachWebStateAt(this, web_state, index);

  ClearOpenersReferencing(index);
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_wrappers_[index]->ReplaceWebState(nullptr);
  web_state_wrappers_.erase(web_state_wrappers_.begin() + index);

  // Update the active index to prevent observer from seeing an invalid WebState
  // as the active one but only send the WebStateActivatedAt notification after
  // the WebStateDetachedAt one.
  bool active_web_state_was_closed = (index == active_index_);
  if (active_index_ > index) {
    --active_index_;
  } else if (active_index_ == index) {
    if (new_active_index != kInvalidIndex && !ContainsIndex(new_active_index)) {
      // TODO(crbug.com/877792): This is a speculative fix for 877792 and short
      // term fix for 960628.
      active_index_ = count() - 1;
    } else {
      active_index_ = new_active_index;
    }
  }

  for (auto& observer : observers_)
    observer.WebStateDetachedAt(this, web_state, index);

  if (active_web_state_was_closed) {
    NotifyIfActiveWebStateChanged(web_state,
                                  WebStateListObserver::CHANGE_REASON_NONE);
  }

  delegate_->WebStateDetached(web_state);
  return detached_web_state;
}

void WebStateList::CloseWebStateAt(int index, int close_flags) {
  // Lock after detaching, since that has its own lock.
  auto detached_web_state = DetachWebStateAt(index);

  CHECK(!locked_);
  base::AutoReset<bool> scoped_lock(&locked_, /* locked */ true);
  const bool user_action = IsClosingFlagSet(close_flags, CLOSE_USER_ACTION);
  for (auto& observer : observers_) {
    observer.WillCloseWebStateAt(this, detached_web_state.get(), index,
                                 user_action);
  }

  detached_web_state.reset();
}

void WebStateList::CloseAllWebStates(int close_flags) {
  PerformBatchOperation(base::BindOnce(
      [](int close_flags, WebStateList* web_state_list) {
        while (!web_state_list->empty())
          web_state_list->CloseWebStateAt(web_state_list->count() - 1,
                                          close_flags);
      },
      close_flags));
}

void WebStateList::ActivateWebStateAt(int index) {
  DCHECK(ContainsIndex(index));
  web::WebState* old_web_state = GetActiveWebState();
  active_index_ = index;
  NotifyIfActiveWebStateChanged(
      old_web_state, WebStateListObserver::CHANGE_REASON_USER_ACTION);
}

void WebStateList::AddObserver(WebStateListObserver* observer) {
  observers_.AddObserver(observer);
}

void WebStateList::RemoveObserver(WebStateListObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebStateList::PerformBatchOperation(
    base::OnceCallback<void(WebStateList*)> operation) {
  for (auto& observer : observers_)
    observer.WillBeginBatchOperation(this);
  if (!operation.is_null())
    std::move(operation).Run(this);
  for (auto& observer : observers_)
    observer.BatchOperationEnded(this);
}

void WebStateList::ClearOpenersReferencing(int index) {
  web::WebState* old_web_state = web_state_wrappers_[index]->web_state();
  for (auto& web_state_wrapper : web_state_wrappers_) {
    if (web_state_wrapper->opener().opener == old_web_state)
      web_state_wrapper->set_opener(WebStateOpener());
  }
}

void WebStateList::NotifyIfActiveWebStateChanged(web::WebState* old_web_state,
                                                 int reason) {
  web::WebState* new_web_state = GetActiveWebState();
  if (old_web_state == new_web_state)
    return;

  for (auto& observer : observers_) {
    observer.WebStateActivatedAt(this, old_web_state, new_web_state,
                                 active_index_, reason);
  }
}

int WebStateList::GetIndexOfNthWebStateOpenedBy(const web::WebState* opener,
                                                int start_index,
                                                bool use_group,
                                                int n) const {
  DCHECK_GT(n, 0);
  if (!opener || !ContainsIndex(start_index) || start_index == INT_MAX)
    return kInvalidIndex;

  const int opener_navigation_index =
      use_group ? opener->GetNavigationManager()->GetLastCommittedItemIndex()
                : -1;

  int found_index = kInvalidIndex;
  for (int index = start_index + 1; index < count() && n; ++index) {
    if (web_state_wrappers_[index]->WasOpenedBy(opener, opener_navigation_index,
                                                use_group)) {
      found_index = index;
      --n;
    } else if (found_index != kInvalidIndex) {
      return found_index;
    }
  }

  return found_index;
}

// static
const int WebStateList::kInvalidIndex;
