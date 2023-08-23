// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace ios {
// Owns the bookmark sync service for bookmarks that belong to the primary
// account.
class AccountBookmarkSyncServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static sync_bookmarks::BookmarkSyncService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static AccountBookmarkSyncServiceFactory* GetInstance();

  AccountBookmarkSyncServiceFactory(const AccountBookmarkSyncServiceFactory&) =
      delete;
  AccountBookmarkSyncServiceFactory& operator=(
      const AccountBookmarkSyncServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AccountBookmarkSyncServiceFactory>;

  AccountBookmarkSyncServiceFactory();
  ~AccountBookmarkSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_SYNC_SERVICE_FACTORY_H_
