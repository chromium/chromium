// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"

#include "base/memory/ref_counted.h"
#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "components/sync_sessions/tab_node_pool.h"
#import "ios/chrome/browser/complex_tasks/ios_task_tab_helper.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#include "ios/web/public/favicon/favicon_status.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::NavigationItem;

namespace {

// Helper to access the correct NavigationItem, accounting for pending entries.
// May return null in rare cases such as a FORWARD_BACK navigation cancelling a
// slow-loading navigation.
NavigationItem* GetPossiblyPendingItemAtIndex(web::WebState* web_state, int i) {
  int pending_index = web_state->GetNavigationManager()->GetPendingItemIndex();
  return (pending_index == i)
             ? web_state->GetNavigationManager()->GetPendingItem()
             : web_state->GetNavigationManager()->GetItemAtIndex(i);
}

}  // namespace

IOSChromeSyncedTabDelegate::IOSChromeSyncedTabDelegate(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state);
}

IOSChromeSyncedTabDelegate::~IOSChromeSyncedTabDelegate() {}

SessionID IOSChromeSyncedTabDelegate::GetWindowId() const {
  return IOSChromeSessionTabHelper::FromWebState(web_state_)->window_id();
}

SessionID IOSChromeSyncedTabDelegate::GetSessionId() const {
  return IOSChromeSessionTabHelper::FromWebState(web_state_)->session_id();
}

bool IOSChromeSyncedTabDelegate::IsBeingDestroyed() const {
  return web_state_->IsBeingDestroyed();
}

std::string IOSChromeSyncedTabDelegate::GetExtensionAppId() const {
  return std::string();
}

bool IOSChromeSyncedTabDelegate::IsInitialBlankNavigation() const {
  if (GetSessionStorageIfNeeded()) {
    return session_storage_.itemStorages.count == 0;
  }
  return web_state_->GetNavigationManager()->GetItemCount() == 0;
}

int IOSChromeSyncedTabDelegate::GetCurrentEntryIndex() const {
  if (GetSessionStorageIfNeeded()) {
    NSInteger lastCommittedIndex = session_storage_.lastCommittedItemIndex;
    if (lastCommittedIndex < 0 ||
        lastCommittedIndex >=
            static_cast<NSInteger>(session_storage_.itemStorages.count)) {
      // It has been observed that lastCommittedIndex can be invalid (see
      // crbug.com/1060553). Returning an invalid index will cause a crash.
      // If lastCommittedIndex is invalid, consider the last index as the
      // current one.
      // As GetSessionStorageIfNeeded just returned true,
      // session_storage_.itemStorages.count is not 0 and
      // session_storage_.itemStorages.count - 1 is valid.
      return session_storage_.itemStorages.count - 1;
    }
    return session_storage_.lastCommittedItemIndex;
  }
  return web_state_->GetNavigationManager()->GetLastCommittedItemIndex();
}

int IOSChromeSyncedTabDelegate::GetEntryCount() const {
  if (GetSessionStorageIfNeeded()) {
    return session_storage_.itemStorages.count;
  }
  return web_state_->GetNavigationManager()->GetItemCount();
}

GURL IOSChromeSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  if (GetSessionStorageIfNeeded()) {
    DCHECK_GE(i, 0);
    NSArray* item_storages = session_storage_.itemStorages;
    DCHECK_LT(i, static_cast<int>(item_storages.count));
    CRWNavigationItemStorage* item = item_storages[i];
    return item.virtualURL;
  }
  NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  return item ? item->GetVirtualURL() : GURL();
}

GURL IOSChromeSyncedTabDelegate::GetFaviconURLAtIndex(int i) const {
  if (GetSessionStorageIfNeeded()) {
    DCHECK_GE(i, 0);
    return GURL();
  }
  NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  return (item && item->GetFavicon().valid ? item->GetFavicon().url : GURL());
}

ui::PageTransition IOSChromeSyncedTabDelegate::GetTransitionAtIndex(
    int i) const {
  if (GetSessionStorageIfNeeded()) {
    DCHECK_GE(i, 0);
    return ui::PAGE_TRANSITION_LINK;
  }
  NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  // If no item exists, there's no coherent PageTransition to be supplied.
  // There's also no ui::PAGE_TRANSITION_UNKNOWN, so let's use the default,
  // which is PAGE_TRANSITION_LINK.
  return item ? item->GetTransitionType() : ui::PAGE_TRANSITION_LINK;
}

std::string IOSChromeSyncedTabDelegate::GetPageLanguageAtIndex(int i) const {
  // TODO(crbug.com/957657): Add page language to NavigationItem.
  return std::string();
}

void IOSChromeSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  if (GetSessionStorageIfNeeded()) {
    NSArray* item_storages = session_storage_.itemStorages;
    DCHECK_GE(i, 0);
    DCHECK_LT(i, static_cast<int>(item_storages.count));
    CRWNavigationItemStorage* item = item_storages[i];
    *serialized_entry =
        sessions::IOSSerializedNavigationBuilder::FromNavigationStorageItem(
            i, item);
    return;
  }
  NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  if (item) {
    *serialized_entry =
        sessions::IOSSerializedNavigationBuilder::FromNavigationItem(i, *item);
  }
}

bool IOSChromeSyncedTabDelegate::ProfileIsSupervised() const {
  return false;
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
IOSChromeSyncedTabDelegate::GetBlockedNavigations() const {
  NOTREACHED();
  return nullptr;
}

bool IOSChromeSyncedTabDelegate::IsPlaceholderTab() const {
  // GetSessionId is not restored so the tab get a new session ID.
  // Placeholder tabs cannot be used.
  return false;
}

bool IOSChromeSyncedTabDelegate::ShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  if (sessions_client->GetSyncedWindowDelegatesGetter()->FindById(
          GetWindowId()) == nullptr)
    return false;

  if (IsInitialBlankNavigation())
    return false;  // This deliberately ignores a new pending entry.

  int entry_count = GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    const GURL& virtual_url = GetVirtualURLAtIndex(i);
    if (!virtual_url.is_valid())
      continue;

    if (sessions_client->ShouldSyncURL(virtual_url))
      return true;
  }
  return false;
}

int64_t IOSChromeSyncedTabDelegate::GetTaskIdForNavigationId(int nav_id) const {
  const IOSTaskTabHelper* ios_task_tab_helper = this->ios_task_tab_helper();
  if (ios_task_tab_helper &&
      ios_task_tab_helper->GetContextRecordTaskId(nav_id) != nullptr) {
    return ios_task_tab_helper->GetContextRecordTaskId(nav_id)->task_id();
  }
  return -1;
}

int64_t IOSChromeSyncedTabDelegate::GetParentTaskIdForNavigationId(
    int nav_id) const {
  const IOSTaskTabHelper* ios_task_tab_helper = this->ios_task_tab_helper();
  if (ios_task_tab_helper &&
      ios_task_tab_helper->GetContextRecordTaskId(nav_id) != nullptr) {
    return ios_task_tab_helper->GetContextRecordTaskId(nav_id)
        ->parent_task_id();
  }
  return -1;
}

int64_t IOSChromeSyncedTabDelegate::GetRootTaskIdForNavigationId(
    int nav_id) const {
  const IOSTaskTabHelper* ios_task_tab_helper = this->ios_task_tab_helper();
  if (ios_task_tab_helper &&
      ios_task_tab_helper->GetContextRecordTaskId(nav_id) != nullptr) {
    return ios_task_tab_helper->GetContextRecordTaskId(nav_id)->root_task_id();
  }
  return -1;
}

const IOSTaskTabHelper* IOSChromeSyncedTabDelegate::ios_task_tab_helper()
    const {
  if (web_state_ == nullptr)
    return nullptr;
  return IOSTaskTabHelper::FromWebState(web_state_);
}

bool IOSChromeSyncedTabDelegate::GetSessionStorageIfNeeded() const {
  // With slim navigation, the navigation manager is only restored when the tab
  // is displayed. Before restoration, the session storage must be used.
  bool should_use_storage =
      web_state_->GetNavigationManager()->IsRestoreSessionInProgress();
  bool storage_has_navigation_items = false;
  if (should_use_storage) {
    if (!session_storage_) {
      session_storage_ = web_state_->BuildSessionStorage();
    }
    storage_has_navigation_items = session_storage_.itemStorages.count;
#if DCHECK_IS_ON()
    if (storage_has_navigation_items) {
      DCHECK_GE(session_storage_.lastCommittedItemIndex, 0);
      DCHECK_LT(session_storage_.lastCommittedItemIndex,
                static_cast<int>(session_storage_.itemStorages.count));
    }
#endif
  }
  return should_use_storage && storage_has_navigation_items;
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSChromeSyncedTabDelegate)
