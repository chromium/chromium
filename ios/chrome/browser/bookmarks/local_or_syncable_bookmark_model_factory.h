// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_LOCAL_OR_SYNCABLE_BOOKMARK_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_LOCAL_OR_SYNCABLE_BOOKMARK_MODEL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
}

namespace ios {
// Owns local/syncable BookmarkModels.
class LocalOrSyncableBookmarkModelFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static bookmarks::BookmarkModel* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static bookmarks::BookmarkModel* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static LocalOrSyncableBookmarkModelFactory* GetInstance();

  LocalOrSyncableBookmarkModelFactory(
      const LocalOrSyncableBookmarkModelFactory&) = delete;
  LocalOrSyncableBookmarkModelFactory& operator=(
      const LocalOrSyncableBookmarkModelFactory&) = delete;

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<LocalOrSyncableBookmarkModelFactory>;

  LocalOrSyncableBookmarkModelFactory();
  ~LocalOrSyncableBookmarkModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_LOCAL_OR_SYNCABLE_BOOKMARK_MODEL_FACTORY_H_
