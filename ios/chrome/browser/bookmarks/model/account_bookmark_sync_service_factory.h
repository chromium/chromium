// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace ios {
// Owns the bookmark sync service for bookmarks that belong to the associated
// profile.
class AccountBookmarkSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static sync_bookmarks::BookmarkSyncService* GetForProfile(
      ProfileIOS* profile);
  static AccountBookmarkSyncServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountBookmarkSyncServiceFactory>;

  AccountBookmarkSyncServiceFactory();
  ~AccountBookmarkSyncServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_
