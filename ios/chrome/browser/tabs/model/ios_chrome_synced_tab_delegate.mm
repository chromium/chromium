// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"

#import "base/check.h"
#import "components/prefs/pref_service.h"
#import "components/sessions/ios/ios_serialized_navigation_builder.h"
#import "components/sync/base/features.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/synced_window_delegates_getter.h"
#import "ios/chrome/browser/complex_tasks/model/ios_task_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// The minimum time between two sync updates of `last_active_time` when the tab
// hasn't changed.
constexpr base::TimeDelta kSyncActiveTimeThreshold = base::Minutes(10);

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

// Returns the time of the most recent activity for `web_state`, which is the
// maximum of the last navigation timestamp (if available) and the last active
// time (i.e. the last time the tab was made visible).
base::Time GetMostRecentActivityTime(const web::WebState* web_state) {
  base::Time result = web_state->GetLastActiveTime();
  if (web_state->IsRealized()) {
    web::NavigationItem* last_committed_item =
        web_state->GetNavigationManager()->GetLastCommittedItem();
    if (last_committed_item) {
      result = std::max(result, last_committed_item->GetTimestamp());
    }
  }
  return result;
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
  if (cached_last_active_time_.has_value() &&
      last_active_time - cached_last_active_time_.value() <
          kSyncActiveTimeThreshold) {
    return cached_last_active_time_.value();
  }
  cached_last_active_time_ = last_active_time;
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
  // A tab is considered as "placeholder" if it is not fully
  // loaded. This corresponds to "unrealized" tabs.
  return !web_state_->IsRealized();
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

  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // If fast account switching via the account particle disk on the NTP is
    // enabled, then for managed accounts, only sync tabs that have been updated
    // after the signin.
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(profile);
    if (auth_service && auth_service->HasPrimaryIdentityManaged(
                            signin::ConsentLevel::kSignin)) {
      base::Time signin_time =
          profile->GetPrefs()->GetTime(prefs::kLastSigninTimestamp);
      // Note: Don't use GetLastActiveTime() here: (a) it only tracks when the
      // tab was last made visible (not when it was last used), and (b) it
      // intentionally caches outdated values for a few minutes. Instead, query
      // the most-recent activity time from the WebState directly.
      if (GetMostRecentActivityTime(web_state_) < signin_time) {
        return false;
      }
    }
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
