// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BrowserDownloadService;
class ProfileIOS;

// Singleton that creates BrowserDownloadService and associates that service
// with a Profile.
class BrowserDownloadServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static BrowserDownloadService* GetForProfile(ProfileIOS* profile);
  static BrowserDownloadServiceFactory* GetInstance();

  // Returns a default testing factory.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<BrowserDownloadServiceFactory>;

  BrowserDownloadServiceFactory();
  ~BrowserDownloadServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
