// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"

#import "base/check.h"
#import "components/sessions/ios/ios_serialized_navigation_builder.h"
#import "components/sync/base/features.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/synced_window_delegates_getter.h"
#import "ios/chrome/browser/complex_tasks/model/ios_task_tab_helper.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
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

}  // namespace

IOSChromeSyncedTabDelegate::IOSChromeSyncedTabDelegate(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state);
}

IOSChromeSyncedTabDelegate::~IOSChromeSyncedTabDelegate() {}

void IOSChromeSyncedTabDelegate::ResetCachedLastActiveTime() {
  cached_last_active_time_.reset();
}

SessionID IOSChromeSyncedTabDelegate::GetWindowId() const {
  return IOSChromeSessionTabHelper::FromWebState(web_state_)->window_id();
}

SessionID IOSChromeSyncedTabDelegate::GetSessionId() const {
  return IOSChromeSessionTabHelper::FromWebState(web_state_)->session_id();
}

bool IOSChromeSyncedTabDelegate::IsBeingDestroyed() const {
  return web_state_->IsBeingDestroyed();
}

base::Time IOSChromeSyncedTabDelegate::GetLastActiveTime() {
  base::Time last_active_time = web_state_->GetLastActiveTime();
  if (base::FeatureList::IsEnabled(syncer::kSyncSessionOnVisibilityChanged)) {
    if (cached_last_active_time_.has_value() &&
        last_active_time - cached_last_active_time_.value() <
            syncer::kSyncSessionOnVisibilityChangedTimeThreshold.Get()) {
      return cached_last_active_time_.value();
    }
    cached_last_active_time_ = last_active_time;
  }
  return last_active_time;
}

std::string IOSChromeSyncedTabDelegate::GetExtensionAppId() const {
  return std::string();
}

bool IOSChromeSyncedTabDelegate::IsInitialBlankNavigation() const {
  DCHECK(!IsPlaceholderTab());
  return web_state_->GetNavigationItemCount() == 0;
}

int IOSChromeSyncedTabDelegate::GetCurrentEntryIndex() const {
  DCHECK(!IsPlaceholderTab());
  return web_state_->GetNavigationManager()->GetLastCommittedItemIndex();
}

int IOSChromeSyncedTabDelegate::GetEntryCount() const {
  DCHECK(!IsPlaceholderTab());
  return web_state_->GetNavigationItemCount();
}

GURL IOSChromeSyncedTabDelegate::GetVirtualURLAtIndex(int i) const {
  DCHECK(!IsPlaceholderTab());
  web::NavigationItem* item = GetPossiblyPendingItemAtIndex(web_state_, i);
  return item ? item->GetVirtualURL() : GURL();
}

void IOSChromeSyncedTabDelegate::GetSerializedNavigationAtIndex(
    int i,
    sessions::SerializedNavigationEntry* serialized_entry) const {
  DCHECK(!IsPlaceholderTab());
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool IOSChromeSyncedTabDelegate::IsPlaceholderTab() const {
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
    if (sessions_client->ShouldSyncURL(GetVirtualURLAtIndex(i))) {
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
IOSChromeSyncedTabDelegate::ReadPlaceholderTabSnapshotIfItShouldSync(
    sync_sessions::SyncSessionsClient* sessions_client) {
  NOTREACHED_IN_MIGRATION()
      << "ReadPlaceholderTabSnapshotIfItShouldSync is not supported for the "
         "iOS platform.";
  return nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSChromeSyncedTabDelegate)
