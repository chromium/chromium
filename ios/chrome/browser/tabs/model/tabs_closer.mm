// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_closer.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

namespace {

// Information about a tab group.
struct TabGroupInfo {
  const TabGroupRange group_range;
  const tab_groups::TabGroupId tab_group_id;
  const tab_groups::TabGroupVisualData visual_data;
};

// Moves WebStates in range [start; start+count) from `source` to `target`.
void MoveWebStatesInRangeBetweenLists(WebStateList* source,
                                      WebStateList* target,
                                      int start,
                                      int count) {
  DCHECK_GE(start, 0);
  DCHECK_LT(start, source->count());
  DCHECK_LE(start + count, source->count());
  // Only one of the WebStateList can contain pinned tabs.
  CHECK(target->pinned_tabs_count() == 0 || source->pinned_tabs_count() == 0);

  const int old_active_index = source->active_index();
  const int old_pinned_count = source->pinned_tabs_count();
  const int offset = target->count();
  const int end = start + count;

  const OrderControllerSourceFromWebStateList order_controller_source(*source);
  const OrderController order_controller(order_controller_source);
  source->ActivateWebStateAt(order_controller.DetermineNewActiveIndex(
      old_active_index, RemovingIndexes({.start = start, .count = count})));

  // Store the groups info.
  std::vector<TabGroupInfo> groups;
  for (const TabGroup* group : source->GetGroups()) {
    TabGroupRange range = group->range();
    // The group is not in the range of moving items, ignore it.
    if (range.range_end() <= start || end <= range.range_begin()) {
      continue;
    }

    // The current implementation does not support partially closing
    // a group. So assert that the group is fully contained in the
    // range of closed items.
    CHECK(start <= range.range_begin() && range.range_end() <= end);
    range.Move(offset - start);
    groups.push_back(TabGroupInfo{
        .group_range = range,
        .tab_group_id = group->tab_group_id(),
        .visual_data = group->visual_data(),
    });
  }

  for (int n = 0; n < count; ++n) {
    const bool is_pinned = start + n < old_pinned_count;
    std::unique_ptr<web::WebState> web_state = source->DetachWebStateAt(start);

    const WebStateList::InsertionParams params =
        WebStateList::InsertionParams::AtIndex(n + offset)
            .Pinned(is_pinned)
            .Activate(start + n == old_active_index);

    const int insertion_index =
        target->InsertWebState(std::move(web_state), params);
    DCHECK_EQ(n + offset, insertion_index);
  }

  // Restore the groups info.
  for (const auto& group : groups) {
    target->CreateGroup(group.group_range.AsSet(), group.visual_data,
                        group.tab_group_id);
  }
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

  // Closes tabs in range [start; start+count) from `original_browser_` and
  // stores state to allow undoing the operation if needed.
  void CloseTabs(int start, int count);

  // Undoes the close operation performed in `CloseTabs`.
  void Undo();

  // Confirms the close operation performed in `CloseTabs`, deleting the state.
  // This is irreversible and no data can be recovered after this method has
  // been called.
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
};

TabsCloser::UndoStorage::UndoStorage(Browser* browser)
    : original_browser_(browser),
      temporary_browser_(Browser::CreateTemporary(browser->GetBrowserState())) {
  SessionRestorationServiceFactory::GetForBrowserState(
      temporary_browser_->GetBrowserState())
      ->AttachBackup(original_browser_.get(), temporary_browser_.get());
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
  openers_.clear();
}

void TabsCloser::UndoStorage::Drop() {
  // Pretend that the original Browser's WebStateList is going through a
  // batched operation. This is a fix for https://crbug.com/1521867 where
  // RecentTabsMediator observes the TabRestoreService for modifications
  // and updates its state each time it is notified by the service.
  //
  // Using a ScopedBatchOperation causes RecentTabsMediator to consider
  // that a batch operation is in progress for one of the WebStateList
  // it is observing and to avoid updating its state for each closed
  // WebState.
  WebStateList::ScopedBatchOperation original_browser_lock =
      original_browser_->GetWebStateList()->StartBatchOperation();

  CloseAllWebStates(*temporary_browser_->GetWebStateList(),
                    WebStateList::CLOSE_USER_ACTION);

  openers_.clear();
}

TabsCloser::TabsCloser(Browser* browser, ClosePolicy policy)
    : browser_(browser), close_policy_(policy) {
  DCHECK(browser_);
  DCHECK(!browser_->GetBrowserState()->IsOffTheRecord());
}

TabsCloser::~TabsCloser() = default;

bool TabsCloser::CanCloseTabs() const {
  WebStateList* web_state_list = browser_->GetWebStateList();
  switch (close_policy_) {
    case ClosePolicy::kAllTabs:
      return web_state_list->count() != 0;

    case ClosePolicy::kRegularTabs:
      return web_state_list->regular_tabs_count() != 0;
  }
}

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

  // Force a session save to avoid having to wait for the timeout.
  // This is mostly useful when user has a large number of tabs (see
  // bug https://crbug.com/1510953 for details).
  SessionRestorationServiceFactory::GetForBrowserState(
      browser_->GetBrowserState())
      ->SaveSessions();

  return state_->count();
}

bool TabsCloser::CanUndoCloseTabs() const {
  return state_ != nullptr;
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
