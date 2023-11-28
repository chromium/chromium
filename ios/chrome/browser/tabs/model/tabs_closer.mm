// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_closer.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/strcat.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/sessions/session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

namespace {

// Moves WebStates in range [start; start+count( from `source` to `target`.
void MoveWebStatesInRangeBetweenLists(WebStateList* source,
                                      WebStateList* target,
                                      int start,
                                      int count) {
  DCHECK_GE(start, 0);
  DCHECK_LT(start, source->count());
  DCHECK_LE(start + count, source->count());

  const int old_active_index = source->active_index();
  const int old_pinned_count = source->pinned_tabs_count();
  const int offset = target->count();

  const OrderControllerSourceFromWebStateList order_controller_source(*source);
  const OrderController order_controller(order_controller_source);
  source->ActivateWebStateAt(order_controller.DetermineNewActiveIndex(
      old_active_index, RemovingIndexes::Range(start, count)));

  for (int n = 0; n < count; ++n) {
    const bool is_pinned = start + n < old_pinned_count;
    std::unique_ptr<web::WebState> web_state = source->DetachWebStateAt(start);

    const int insertion_flags =
        (is_pinned ? WebStateList::INSERT_PINNED : 0) |
        (start + n == old_active_index ? WebStateList::INSERT_ACTIVATE : 0);

    const int insertion_index = target->InsertWebState(
        n + offset, std::move(web_state), insertion_flags, WebStateOpener());

    DCHECK_EQ(n + offset, insertion_index);
  }
}

// Get identifier for temporary Browser from Browser.
std::string GetTemporaryIdentifier(Browser* original_browser) {
  using session_util::GetSessionIdentifier;
  return base::StrCat({GetSessionIdentifier(original_browser), "/{Undo}"});
}

}  // namespace

class TabsCloser::UndoStorage {
 public:
  explicit UndoStorage(Browser* browser);

  UndoStorage(const UndoStorage&) = delete;
  UndoStorage& operator=(const UndoStorage&) = delete;

  ~UndoStorage();

  // Returns the number of tabs that have been closed.
  int count() const { return temporary_browser_->GetWebStateList()->count(); }

  // Closes tabs in range [start; start+count( from `original_browser_` and
  // stores state to allow undoing the operation if needed.
  void CloseTabs(int start, int count);

  // Undo the close operation performed in the constructor.
  void Undo();

  // Confirm the close operation performed in the constructor, deleting
  // the state. This is irreversible and no data can be recovered after
  // this method has been called.
  void Drop();

 private:
  // Stores opener-opened information for a WebState.
  struct Opener {
    int opener_index;
    int opener_navigation_index;
  };

  raw_ptr<Browser> original_browser_{nullptr};
  std::unique_ptr<Browser> temporary_browser_;
  std::vector<std::optional<Opener>> openers_;
  const std::string identifier_;
};

TabsCloser::UndoStorage::UndoStorage(Browser* browser)
    : original_browser_(browser),
      temporary_browser_(Browser::CreateTemporary(browser->GetBrowserState())),
      identifier_(GetTemporaryIdentifier(browser)) {
  SessionRestorationServiceFactory::GetForBrowserState(
      temporary_browser_->GetBrowserState())
      ->SetSessionID(temporary_browser_.get(), identifier_);
}

TabsCloser::UndoStorage::~UndoStorage() {
  // If there is still a pending undo when the object is destroyed, consider
  // that the close operation has been confirmed.
  Drop();

  // The temporary browser must now be empty.
  DCHECK(temporary_browser_->GetWebStateList()->empty());

  SessionRestorationService* service =
      SessionRestorationServiceFactory::GetForBrowserState(
          temporary_browser_->GetBrowserState());

  service->Disconnect(temporary_browser_.get());
  service->DeleteDataForDiscardedSessions({identifier_}, base::DoNothing());
}

void TabsCloser::UndoStorage::CloseTabs(int start, int count) {
  WebStateList* web_state_list = original_browser_->GetWebStateList();
  std::map<web::WebState*, int> web_state_map;
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    web_state_map.insert(std::make_pair(web_state, index));
  }

  for (int index = 0; index < web_state_list->count(); ++index) {
    WebStateOpener opener = web_state_list->GetOpenerOfWebStateAt(index);
    if (opener.opener) {
      DCHECK(base::Contains(web_state_map, opener.opener));
      openers_.push_back(Opener{
          .opener_index = web_state_map[opener.opener],
          .opener_navigation_index = opener.navigation_index,
      });
    } else {
      openers_.push_back(std::nullopt);
    }
  }

  DCHECK_EQ(static_cast<int>(openers_.size()), web_state_list->count());

  WebStateList* source = original_browser_->GetWebStateList();
  WebStateList* target = temporary_browser_->GetWebStateList();

  WebStateList::ScopedBatchOperation lock_source =
      source->StartBatchOperation();
  WebStateList::ScopedBatchOperation lock_target =
      target->StartBatchOperation();
  MoveWebStatesInRangeBetweenLists(source, target, start, count);
}

void TabsCloser::UndoStorage::Undo() {
  WebStateList* source = temporary_browser_->GetWebStateList();
  WebStateList* target = original_browser_->GetWebStateList();

  WebStateList::ScopedBatchOperation lock_source =
      source->StartBatchOperation();
  WebStateList::ScopedBatchOperation lock_target =
      target->StartBatchOperation();
  MoveWebStatesInRangeBetweenLists(source, target, 0, source->count());

  DCHECK_EQ(static_cast<int>(openers_.size()), target->count());
  for (int index = 0; index < target->count(); ++index) {
    const std::optional<Opener>& opener = openers_[index];
    if (opener.has_value()) {
      target->SetOpenerOfWebStateAt(
          index, WebStateOpener(target->GetWebStateAt(opener->opener_index),
                                opener->opener_navigation_index));
    }
  }
}

void TabsCloser::UndoStorage::Drop() {
  temporary_browser_->GetWebStateList()->CloseAllWebStates(
      WebStateList::CLOSE_USER_ACTION);
}

TabsCloser::TabsCloser(Browser* browser, ClosePolicy policy)
    : browser_(browser), close_policy_(policy) {
  DCHECK(browser_);
  DCHECK(!browser_->GetBrowserState()->IsOffTheRecord());
}

TabsCloser::~TabsCloser() = default;

int TabsCloser::CloseTabs() {
  DCHECK(CanCloseTabs());

  WebStateList* web_state_list = browser_->GetWebStateList();

  int start, count;
  switch (close_policy_) {
    case ClosePolicy::kAllTabs:
      start = 0;
      count = web_state_list->count();
      break;

    case ClosePolicy::kRegularTabs:
      start = web_state_list->pinned_tabs_count();
      count = web_state_list->regular_tabs_count();
      break;
  }

  state_ = std::make_unique<UndoStorage>(browser_);
  state_->CloseTabs(start, count);
  return state_->count();
}

int TabsCloser::UndoCloseTabs() {
  DCHECK(CanUndoCloseTabs());
  const int result = state_->count();
  state_->Undo();
  state_.reset();
  return result;
}

int TabsCloser::ConfirmDeletion() {
  DCHECK(CanUndoCloseTabs());
  const int result = state_->count();
  state_->Drop();
  state_.reset();
  return result;
}

bool TabsCloser::CanCloseTabs() const {
  WebStateList* web_state_list = browser_->GetWebStateList();
  switch (close_policy_) {
    case ClosePolicy::kAllTabs:
      return web_state_list->count() != 0;

    case ClosePolicy::kRegularTabs:
      return web_state_list->regular_tabs_count() != 0;
  }
}

bool TabsCloser::CanUndoCloseTabs() const {
  return state_ != nullptr;
}
