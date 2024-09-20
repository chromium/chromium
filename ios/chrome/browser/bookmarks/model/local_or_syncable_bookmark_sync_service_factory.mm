// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
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
LocalOrSyncableBookmarkSyncServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
sync_bookmarks::BookmarkSyncService*
LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
LocalOrSyncableBookmarkSyncServiceFactory*
LocalOrSyncableBookmarkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<LocalOrSyncableBookmarkSyncServiceFactory> instance;
  return instance.get();
}

LocalOrSyncableBookmarkSyncServiceFactory::
    LocalOrSyncableBookmarkSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "LocalOrSyncableBookmarkSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
}

LocalOrSyncableBookmarkSyncServiceFactory::
    ~LocalOrSyncableBookmarkSyncServiceFactory() {}

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

web::BrowserState*
LocalOrSyncableBookmarkSyncServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
