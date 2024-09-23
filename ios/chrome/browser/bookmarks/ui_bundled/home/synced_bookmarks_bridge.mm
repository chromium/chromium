// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/synced_bookmarks_bridge.h"

#import "base/memory/weak_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace sync_bookmarks {

#pragma mark - SyncedBookmarksObserverBridge

SyncedBookmarksObserverBridge::SyncedBookmarksObserverBridge(
    id<SyncObserverModelBridge> delegate,
    ProfileIOS* profile)
    : SyncObserverBridge(delegate, SyncServiceFactory::GetForProfile(profile)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      profile_(profile->AsWeakPtr()) {}

SyncedBookmarksObserverBridge::~SyncedBookmarksObserverBridge() {}

#pragma mark - Signin and syncing status

bool SyncedBookmarksObserverBridge::IsPerformingInitialSync() {
  CHECK(profile_.get());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_.get());

  return sync_service->GetTypesWithPendingDownloadForInitialSync().Has(
      syncer::BOOKMARKS);
}

}  // namespace sync_bookmarks
