// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"

#include "components/bookmarks/common/bookmark_features.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
sync_bookmarks::BookmarkSyncService*
AccountBookmarkSyncServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<sync_bookmarks::BookmarkSyncService>(
          profile, /*create=*/true);
}

// static
AccountBookmarkSyncServiceFactory*
AccountBookmarkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<AccountBookmarkSyncServiceFactory> instance;
  return instance.get();
}

AccountBookmarkSyncServiceFactory::AccountBookmarkSyncServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AccountBookmarkSyncService",
                                    ProfileSelection::kRedirectedInIncognito) {}

AccountBookmarkSyncServiceFactory::~AccountBookmarkSyncServiceFactory() =
    default;

std::unique_ptr<KeyedService>
AccountBookmarkSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service(
      new sync_bookmarks::BookmarkSyncService(
          syncer::WipeModelUponSyncDisabledBehavior::kAlways));
  return bookmark_sync_service;
}

}  // namespace ios
