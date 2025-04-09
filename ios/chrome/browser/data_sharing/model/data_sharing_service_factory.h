// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace data_sharing {

class DataSharingService;

// Factory for DataSharingService.
class DataSharingServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DataSharingService* GetForProfile(ProfileIOS* profile);

  static DataSharingServiceFactory* GetInstance();

  // Returns the default factory used to build DataSharingService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<DataSharingServiceFactory>;

  DataSharingServiceFactory();
  ~DataSharingServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_FACTORY_H_
