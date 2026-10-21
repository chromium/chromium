// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"

#include <utility>

#include "components/bookmarks/common/bookmark_features.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

AccountBookmarkSyncServiceFactory::AccountBookmarkSyncServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AccountBookmarkSyncService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
AccountBookmarkSyncServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<sync_bookmarks::BookmarkSyncService>(
      syncer::WipeModelUponSyncDisabledBehavior::kAlways);
}

}  // namespace ios
