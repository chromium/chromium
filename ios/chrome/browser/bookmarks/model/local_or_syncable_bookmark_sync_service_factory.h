// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#import "components/sync_bookmarks/bookmark_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns the bookmark sync service.
class LocalOrSyncableBookmarkSyncServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          LocalOrSyncableBookmarkSyncServiceFactory,
          sync_bookmarks::BookmarkSyncService> {
 public:
  explicit LocalOrSyncableBookmarkSyncServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_
