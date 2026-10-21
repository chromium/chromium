// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_

#include "ios/chrome/browser/download/model/browser_download_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that creates BrowserDownloadService and associates that service
// with a Profile.
class BrowserDownloadServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<BrowserDownloadServiceFactory,
                                                BrowserDownloadService> {
 public:
  explicit BrowserDownloadServiceFactory(PassKey key);

  // Returns a default testing factory.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
