// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace ios {
// Singleton that owns the bookmark sync service.
class LocalOrSyncableBookmarkSyncServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static sync_bookmarks::BookmarkSyncService* GetForBrowserState(
      ProfileIOS* profile);

  static sync_bookmarks::BookmarkSyncService* GetForProfile(
      ProfileIOS* profile);
  static LocalOrSyncableBookmarkSyncServiceFactory* GetInstance();

  LocalOrSyncableBookmarkSyncServiceFactory(
      const LocalOrSyncableBookmarkSyncServiceFactory&) = delete;
  LocalOrSyncableBookmarkSyncServiceFactory& operator=(
      const LocalOrSyncableBookmarkSyncServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<LocalOrSyncableBookmarkSyncServiceFactory>;

  LocalOrSyncableBookmarkSyncServiceFactory();
  ~LocalOrSyncableBookmarkSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_
