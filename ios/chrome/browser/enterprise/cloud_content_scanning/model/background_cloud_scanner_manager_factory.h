// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise_connectors {

class BackgroundCloudScannerManager;

class BackgroundCloudScannerManagerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of BackgroundCloudScannerManagerFactory.
  static BackgroundCloudScannerManagerFactory* GetInstance();

  // Returns the BackgroundCloudScannerManager for `profile`, creating it if it
  // is not yet created.
  static BackgroundCloudScannerManager* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<BackgroundCloudScannerManagerFactory>;

  BackgroundCloudScannerManagerFactory();
  ~BackgroundCloudScannerManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_BACKGROUND_CLOUD_SCANNER_MANAGER_FACTORY_H_
