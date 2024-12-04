// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"

#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

namespace ios {

namespace {

syncer::WipeModelUponSyncDisabledBehavior
GetWipeModelUponSyncDisabledBehavior() {
  if (IsFirstSessionAfterDeviceRestore() != signin::Tribool::kTrue) {
    return syncer::WipeModelUponSyncDisabledBehavior::kNever;
  }
  return syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata;
}

}  // namespace

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
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
}

LocalOrSyncableBookmarkSyncServiceFactory::
    ~LocalOrSyncableBookmarkSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
LocalOrSyncableBookmarkSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service(
      new sync_bookmarks::BookmarkSyncService(
          BookmarkUndoServiceFactory::GetForProfileIfExists(profile),
          GetWipeModelUponSyncDisabledBehavior()));
  return bookmark_sync_service;
}

}  // namespace ios
