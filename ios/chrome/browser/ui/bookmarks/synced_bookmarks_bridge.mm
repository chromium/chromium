// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/synced_bookmarks_bridge.h"

#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sync_bookmarks {

#pragma mark - SyncedBookmarksObserverBridge

SyncedBookmarksObserverBridge::SyncedBookmarksObserverBridge(
    id<SyncObserverModelBridge> delegate,
    ChromeBrowserState* browserState)
    : SyncObserverBridge(delegate,
                         SyncServiceFactory::GetForBrowserState(browserState)),
      identity_manager_(
          IdentityManagerFactory::GetForBrowserState(browserState)),
      browser_state_(browserState) {}

SyncedBookmarksObserverBridge::~SyncedBookmarksObserverBridge() {}

#pragma mark - Signin and syncing status

bool SyncedBookmarksObserverBridge::HasSyncConsent() {
  return identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

bool SyncedBookmarksObserverBridge::IsPerformingInitialSync() {
  if (!HasSyncConsent()) {
    return false;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state_);

  PrefService* user_pref_service = browser_state_->GetPrefs();
  bool is_managed =
      user_pref_service->FindPreference(syncer::prefs::kSyncBookmarks)
          ->IsManaged();

  // If bookmarks are enterprise managed (i.e. disabled) then an initial sync
  // never happens.
  bool can_sync_start = sync_service->CanSyncFeatureStart() && !is_managed;
  bool no_sync_error =
      sync_service->GetUserSettings()->IsFirstSetupComplete() &&
      sync_service->GetUserActionableError() ==
          syncer::SyncService::UserActionableError::kNone;

  return can_sync_start && no_sync_error &&
         !sync_service->GetActiveDataTypes().Has(syncer::BOOKMARKS);
}

}  // namespace sync_bookmarks
