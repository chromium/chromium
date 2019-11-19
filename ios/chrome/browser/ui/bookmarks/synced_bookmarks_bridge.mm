// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/synced_bookmarks_bridge.h"

#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sync_bookmarks {

#pragma mark - SyncedBookmarksObserverBridge

SyncedBookmarksObserverBridge::SyncedBookmarksObserverBridge(
    id<SyncObserverModelBridge> delegate,
    ios::ChromeBrowserState* browserState)
    : SyncObserverBridge(
          delegate,
          ProfileSyncServiceFactory::GetForBrowserState(browserState)),
      identity_manager_(
          IdentityManagerFactory::GetForBrowserState(browserState)),
      browser_state_(browserState) {}

SyncedBookmarksObserverBridge::~SyncedBookmarksObserverBridge() {}

#pragma mark - Signin and syncing status

bool SyncedBookmarksObserverBridge::IsSignedIn() {
  return identity_manager_->HasPrimaryAccount();
}

bool SyncedBookmarksObserverBridge::IsPerformingInitialSync() {
  if (!IsSignedIn())
    return false;

  SyncSetupService* sync_setup_service =
      SyncSetupServiceFactory::GetForBrowserState(browser_state_);

  bool sync_enabled = sync_setup_service->IsSyncEnabled();
  bool no_sync_error = (sync_setup_service->GetSyncServiceState() ==
                        SyncSetupService::kNoSyncServiceError);

  return sync_enabled && no_sync_error &&
         !sync_setup_service->IsDataTypeActive(syncer::BOOKMARKS);
}

}  // namespace sync_bookmarks
