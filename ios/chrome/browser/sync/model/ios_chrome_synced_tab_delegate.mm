// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/ios_chrome_synced_tab_delegate.h"

#import "base/check.h"
#import "components/sessions/ios/ios_serialized_navigation_builder.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/synced_window_delegates_getter.h"
#import "ios/chrome/browser/complex_tasks/model/ios_task_tab_helper.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/web_state.h"

namespace {

// Helper to access the correct NavigationItem, accounting for pending entries.
// May return null in rare cases such as a FORWARD_BACK navigation cancelling a
// slow-loading navigation.
web::NavigationItem* GetPossiblyPendingItemAtIndex(web::WebState* web_state,
                                                   int index) {
  int pending_index = web_state->GetNavigationManager()->GetPendingItemIndex();
  return (pending_index == index)
             ? web_state->GetNavigationManager()->GetPendingItem()
             : web_state->GetNavigationManager()->GetItemAtIndex(index);
}

// Returns whether placeholder tabs are supported.
bool ArePlaceholderTabsSupported() {
  // The support for placeholder tabs requires the WebState session id to be
  // stable across application restart. It is the case since M-114 which added
  // the code to save and restore the identifier. However, it also requires
  // that the stable identifier is communicated to sync with the detail about
  // the corresponding session which will happen as the application is used.
  //
  // Yet, placeholder tabs support is required to enable session restoration
  // optimisations. As this will be launched later, it is expected that by
  // that point, all existing sessions will have been converted to use the
  // stable session id and the session state uploaded to sync. Thus it is
  // safe to enable the support of placeholder tabs behind the same feature
  // controlling the other session restoration optimisations.
  return web::features::UseSessionSerializationOptimizations();
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

base::Time IOSChromeSyncedTabDelegate::GetLastActiveTime() const {
  return web_state_->GetLastActiveTime();
}

std::string IOSChromeSyncedTabDelegate::GetExtensionAppId() const {
  return std::string();
}

bool IOSChromeSyncedTabDelegate::IsInitialBlankNavigation() const {
  DCHECK(!IsPlaceholderTab());
  if (GetSessionStorageIfNeeded()) {
    return session_storage_.itemStorages.count == 0;
  }
  return web_state_->GetNavigationItemCount() == 0;
}

int IOSChromeSyncedTabDelegate::GetCurrentEntryIndex() const {
  DCHECK(!IsPlaceholderTab());
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
  DCHECK(!IsPlaceholderTab());
  if (GetSessionStorageIfNeeded()) {
    return static_cast<int>(session_storage_.itemStorages.count);
  }
  return web_state_->GetNavigationItemCount();
}

GURL IOSChromeSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  DCHECK(!IsPlaceholderTab());
  if (GetSessionStorageIfNeeded()) {
    DCHECK_GE(i, 0);
    NSArray<CRWNavigationItemStorage*>* item_storages =
        session_storage_.itemStorages;
    DCHECK_LT(i, static_cast<int>(item_storages.count));
    CRWNavigationItemStorage* item = item_storages[i];
    return item.virtualURL;
  }
  web::NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  return item ? item->GetVirtualURL() : GURL();
}

std::string IOSChromeSyncedTabDelegate::GetPageLanguageAtIndex(int i) const {
  // TODO(crbug.com/957657): Add page language to NavigationItem.
  DCHECK(!IsPlaceholderTab());
  return std::string();
}

void IOSChromeSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  DCHECK(!IsPlaceholderTab());
  if (GetSessionStorageIfNeeded()) {
    NSArray<CRWNavigationItemStorage*>* item_storages =
        session_storage_.itemStorages;
    DCHECK_GE(i, 0);
    DCHECK_LT(i, static_cast<int>(item_storages.count));
    CRWNavigationItemStorage* item = item_storages[i];
    *serialized_entry =
        sessions::IOSSerializedNavigationBuilder::FromNavigationStorageItem(
            i, item);
    return;
  }
  web::NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  if (item) {
    *serialized_entry =
        sessions::IOSSerializedNavigationBuilder::FromNavigationItem(i, *item);
  }
}

bool IOSChromeSyncedTabDelegate::ProfileHasChildAccount() const {
  DCHECK(!IsPlaceholderTab());
  return false;
}

const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
IOSChromeSyncedTabDelegate::GetBlockedNavigations() const {
  NOTREACHED();
  return nullptr;
}

bool IOSChromeSyncedTabDelegate::IsPlaceholderTab() const {
  // Can't be a placeholder tab if the support for placeholder tabs is not
  // enabled.
  if (!ArePlaceholderTabsSupported()) {
    return false;
  }

  // A tab is considered as "placeholder" if it is not fully loaded. This
  // corresponds to "unrealized" tabs or tabs that are still restoring their
  // navigation history.
  if (!web_state_->IsRealized()) {
    return true;
  }

  if (web_state_->GetNavigationManager()->IsRestoreSessionInProgress()) {
    return true;
  }

  // The WebState is realized and the navigation history fully loaded, the
  // tab can be considered as valid for sync.
  return false;
}

bool IOSChromeSyncedTabDelegate::ShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  DCHECK(!IsPlaceholderTab());
  if (!sessions_client->GetSyncedWindowDelegatesGetter()->FindById(
          GetWindowId())) {
    return false;
  }

  if (IsInitialBlankNavigation()) {
    return false;  // This deliberately ignores a new pending entry.
  }

  int entry_count = GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    const GURL& virtual_url = GetVirtualURLAtIndex(i);
    if (!virtual_url.is_valid()) {
      continue;
    }

    if (sessions_client->ShouldSyncURL(virtual_url)) {
      return true;
    }
  }
  return false;
}

int64_t IOSChromeSyncedTabDelegate::GetTaskIdForNavigationId(int nav_id) const {
  DCHECK(!IsPlaceholderTab());
  const IOSContentRecordTaskId* record =
      IOSTaskTabHelper::FromWebState(web_state_)
          ->GetContextRecordTaskId(nav_id);
  return record ? record->task_id() : -1;
}

int64_t IOSChromeSyncedTabDelegate::GetParentTaskIdForNavigationId(
    int nav_id) const {
  DCHECK(!IsPlaceholderTab());
  const IOSContentRecordTaskId* record =
      IOSTaskTabHelper::FromWebState(web_state_)
          ->GetContextRecordTaskId(nav_id);
  return record ? record->parent_task_id() : -1;
}

int64_t IOSChromeSyncedTabDelegate::GetRootTaskIdForNavigationId(
    int nav_id) const {
  DCHECK(!IsPlaceholderTab());
  const IOSContentRecordTaskId* record =
      IOSTaskTabHelper::FromWebState(web_state_)
          ->GetContextRecordTaskId(nav_id);
  return record ? record->root_task_id() : -1;
}

std::unique_ptr<sync_sessions::SyncedTabDelegate>
IOSChromeSyncedTabDelegate::CreatePlaceholderTabSyncedTabDelegate() {
  NOTREACHED()
      << "CreatePlaceholderTabSyncedTabDelegate is not supported for the "
         "iOS platform.";
  return nullptr;
}

bool IOSChromeSyncedTabDelegate::GetSessionStorageIfNeeded() const {
  // Never use the session storage when placeholder tabs support is enabled.
  // In fact, using the session storage is a workaround to missing placeholder
  // tab support.
  if (ArePlaceholderTabsSupported()) {
    return false;
  }

  // Unrealized web states should always use session storage, regardless of
  // navigation items.
  if (!web_state_->IsRealized()) {
    if (!session_storage_) {
      session_storage_ = web_state_->BuildSessionStorage();
    }
    return true;
  }

  // With slim navigation, the navigation manager is only restored when the tab
  // is displayed. Before restoration, the session storage must be used.
  bool should_use_storage =
      web_state_->GetNavigationManager()->IsRestoreSessionInProgress();
  bool storage_has_navigation_items = false;
  if (should_use_storage) {
    if (!session_storage_) {
      session_storage_ = web_state_->BuildSessionStorage();
    }
    storage_has_navigation_items = session_storage_.itemStorages.count != 0;
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
