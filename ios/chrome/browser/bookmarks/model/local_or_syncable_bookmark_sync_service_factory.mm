// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"

#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
sync_bookmarks::BookmarkSyncService*
LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<sync_bookmarks::BookmarkSyncService>(
          profile, /*create=*/true);
}

// static
LocalOrSyncableBookmarkSyncServiceFactory*
LocalOrSyncableBookmarkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<LocalOrSyncableBookmarkSyncServiceFactory> instance;
  return instance.get();
}

LocalOrSyncableBookmarkSyncServiceFactory::
    LocalOrSyncableBookmarkSyncServiceFactory()
    : ProfileKeyedServiceFactoryIOS("LocalOrSyncableBookmarkSyncService",
                                    ProfileSelection::kRedirectedInIncognito) {}

LocalOrSyncableBookmarkSyncServiceFactory::
    ~LocalOrSyncableBookmarkSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
LocalOrSyncableBookmarkSyncServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<sync_bookmarks::BookmarkSyncService>(
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
}

}  // namespace ios
