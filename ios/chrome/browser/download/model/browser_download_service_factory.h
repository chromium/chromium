// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class BrowserDownloadService;

// Singleton that creates BrowserDownloadService and associates that service
// with a Profile.
class BrowserDownloadServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static BrowserDownloadService* GetForProfile(ProfileIOS* profile);
  static BrowserDownloadServiceFactory* GetInstance();

  BrowserDownloadServiceFactory(const BrowserDownloadServiceFactory&) = delete;
  BrowserDownloadServiceFactory& operator=(
      const BrowserDownloadServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<BrowserDownloadServiceFactory>;

  BrowserDownloadServiceFactory();
  ~BrowserDownloadServiceFactory() override;

  // BrowserStateKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  web::BrowserState* GetBrowserStateToUse(web::BrowserState*) const override;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
