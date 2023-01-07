// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_FAVICON_LOADER_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_FAVICON_LOADER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class FaviconLoader;

// Singleton that owns all FaviconLoaders and associates them with
// ChromeBrowserState.
class IOSChromeFaviconLoaderFactory : public BrowserStateKeyedServiceFactory {
 public:
  static FaviconLoader* GetForBrowserState(ChromeBrowserState* browser_state);
  static FaviconLoader* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static IOSChromeFaviconLoaderFactory* GetInstance();
  // Returns the default factory used to build FaviconLoader. Can be registered
  // with SetTestingFactory to use the FaviconService instance during testing.
  static TestingFactory GetDefaultFactory();

  IOSChromeFaviconLoaderFactory(const IOSChromeFaviconLoaderFactory&) = delete;
  IOSChromeFaviconLoaderFactory& operator=(
      const IOSChromeFaviconLoaderFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeFaviconLoaderFactory>;

  IOSChromeFaviconLoaderFactory();
  ~IOSChromeFaviconLoaderFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_IOS_CHROME_FAVICON_LOADER_FACTORY_H_
