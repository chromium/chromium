// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_MODEL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class LegacyBookmarkModel;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace ios {

// Owns local/syncable BookmarkModels.
class LocalOrSyncableBookmarkModelFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static LegacyBookmarkModel* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static LegacyBookmarkModel* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);

  // Returns a dedicated BookmarkModel instance for `browser_state` that is
  // guaranteed to not be shared with other factories. Callers must ensure that
  // `syncer::kEnableBookmarkFoldersForAccountStorage` is disabled.
  static bookmarks::BookmarkModel*
  GetDedicatedUnderlyingModelForBrowserStateIfUnificationDisabledOrDie(
      ChromeBrowserState* browser_state);

  LocalOrSyncableBookmarkModelFactory(
      const LocalOrSyncableBookmarkModelFactory&) = delete;
  LocalOrSyncableBookmarkModelFactory& operator=(
      const LocalOrSyncableBookmarkModelFactory&) = delete;

  static LocalOrSyncableBookmarkModelFactory* GetInstance();

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

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LOCAL_OR_SYNCABLE_BOOKMARK_MODEL_FACTORY_H_
