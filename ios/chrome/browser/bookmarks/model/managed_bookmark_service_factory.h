// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace bookmarks {
class ManagedBookmarkService;
}

// Singleton that owns all ManagedBookmarkService and associates them with
// ChromeBrowserState.
class ManagedBookmarkServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static bookmarks::ManagedBookmarkService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static ManagedBookmarkServiceFactory* GetInstance();

  ManagedBookmarkServiceFactory(const ManagedBookmarkServiceFactory&) = delete;
  ManagedBookmarkServiceFactory& operator=(
      const ManagedBookmarkServiceFactory&) = delete;

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<ManagedBookmarkServiceFactory>;

  ManagedBookmarkServiceFactory();
  ~ManagedBookmarkServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
