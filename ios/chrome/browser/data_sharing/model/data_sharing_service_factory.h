// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_FACTORY_H_

#import "components/data_sharing/public/data_sharing_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace data_sharing {

// Factory for DataSharingService.
class DataSharingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<DataSharingServiceFactory,
                                                DataSharingService> {
 public:
  explicit DataSharingServiceFactory(PassKey key);

  // Returns the default factory used to build DataSharingService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_SERVICE_FACTORY_H_
