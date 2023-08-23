// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/features.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/signin_util.h"

namespace ios {

namespace {

// Kill switch as an extra safeguard, in addition to the guarding behind
// syncer::kReplaceSyncPromosWithSignInPromos.
BASE_FEATURE(kAllowBookmarkModelWipingForFirstSessionAfterDeviceRestore,
             "AllowBookmarkModelWipingForFirstSessionAfterDeviceRestore",
             base::FEATURE_ENABLED_BY_DEFAULT);

syncer::WipeModelUponSyncDisabledBehavior
GetWipeModelUponSyncDisabledBehavior() {
  if (IsFirstSessionAfterDeviceRestore() != signin::Tribool::kTrue) {
    return syncer::WipeModelUponSyncDisabledBehavior::kNever;
  }

  return (base::FeatureList::IsEnabled(
              kAllowBookmarkModelWipingForFirstSessionAfterDeviceRestore) &&
          base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos))
             ? syncer::WipeModelUponSyncDisabledBehavior::
                   kOnceIfTrackingMetadata
             : syncer::WipeModelUponSyncDisabledBehavior::kNever;
}

}  // namespace

// static
sync_bookmarks::BookmarkSyncService*
LocalOrSyncableBookmarkSyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service(
      new sync_bookmarks::BookmarkSyncService(
          BookmarkUndoServiceFactory::GetForBrowserStateIfExists(browser_state),
          GetWipeModelUponSyncDisabledBehavior()));
  return bookmark_sync_service;
}

web::BrowserState*
LocalOrSyncableBookmarkSyncServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
