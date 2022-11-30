// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
}

namespace ios {
// Singleton that owns all BookmarkModels and associates them with
// ChromeBrowserState.
class BookmarkModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  static bookmarks::BookmarkModel* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static bookmarks::BookmarkModel* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static BookmarkModelFactory* GetInstance();

  BookmarkModelFactory(const BookmarkModelFactory&) = delete;
  BookmarkModelFactory& operator=(const BookmarkModelFactory&) = delete;

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<BookmarkModelFactory>;

  BookmarkModelFactory();
  ~BookmarkModelFactory() override;

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

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_
