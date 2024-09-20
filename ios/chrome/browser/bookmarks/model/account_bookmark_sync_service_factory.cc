// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"

#include "base/no_destructor.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
sync_bookmarks::BookmarkSyncService*
AccountBookmarkSyncServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
sync_bookmarks::BookmarkSyncService*
AccountBookmarkSyncServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
AccountBookmarkSyncServiceFactory*
AccountBookmarkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<AccountBookmarkSyncServiceFactory> instance;
  return instance.get();
}

AccountBookmarkSyncServiceFactory::AccountBookmarkSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AccountBookmarkSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
}

AccountBookmarkSyncServiceFactory::~AccountBookmarkSyncServiceFactory() =
    default;

std::unique_ptr<KeyedService>
AccountBookmarkSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service(
      new sync_bookmarks::BookmarkSyncService(
          BookmarkUndoServiceFactory::GetForProfileIfExists(profile),
          syncer::WipeModelUponSyncDisabledBehavior::kAlways));
  return bookmark_sync_service;
}

web::BrowserState* AccountBookmarkSyncServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
