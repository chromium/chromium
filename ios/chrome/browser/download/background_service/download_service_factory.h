// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_BACKGROUND_SERVICE_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_BACKGROUND_SERVICE_DOWNLOAD_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace download {
class DownloadService;
}

// Singleton that owns all DownloadServiceFactory and associates them with
// ChromeBrowserState.
class DownloadServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static download::DownloadService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static DownloadServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<DownloadServiceFactory>;

  DownloadServiceFactory();
  ~DownloadServiceFactory() override;
  DownloadServiceFactory(const DownloadServiceFactory&) = delete;
  DownloadServiceFactory& operator=(const DownloadServiceFactory&) = delete;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_BACKGROUND_SERVICE_DOWNLOAD_SERVICE_FACTORY_H_