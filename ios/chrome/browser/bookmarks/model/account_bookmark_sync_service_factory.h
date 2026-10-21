// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Owns the bookmark sync service for bookmarks that belong to the associated
// profile.
class AccountBookmarkSyncServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AccountBookmarkSyncServiceFactory,
          sync_bookmarks::BookmarkSyncService> {
 public:
  explicit AccountBookmarkSyncServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_
