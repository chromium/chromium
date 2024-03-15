// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_MODEL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

class ChromeBrowserState;
class LegacyBookmarkModel;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace ios {

// Owns BookmarkModels associated with the primary account.
class AccountBookmarkModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  static LegacyBookmarkModel* GetForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns a dedicated BookmarkModel instance for `browser_state` that is
  // guaranteed to not be shared with other factories. Callers must ensure that
  // `syncer::kEnableBookmarkFoldersForAccountStorage` is disabled.
  static bookmarks::BookmarkModel*
  GetDedicatedUnderlyingModelForBrowserStateIfUnificationDisabledOrDie(
      ChromeBrowserState* browser_state);

  static AccountBookmarkModelFactory* GetInstance();
  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

  AccountBookmarkModelFactory(const AccountBookmarkModelFactory&) = delete;
  AccountBookmarkModelFactory& operator=(const AccountBookmarkModelFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AccountBookmarkModelFactory>;

  AccountBookmarkModelFactory();
  ~AccountBookmarkModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_ACCOUNT_BOOKMARK_MODEL_FACTORY_H_
