// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_IOS_H_

#import "components/enterprise/browser/identifiers/profile_id_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace enterprise {

class ProfileIdServiceFactoryIOS final
    : public TypedProfileKeyedServiceFactoryIOS<ProfileIdServiceFactoryIOS,
                                                ProfileIdService> {
 public:
  explicit ProfileIdServiceFactoryIOS(PassKey key);

  // Returns the default factory used to build ProfileIdService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace enterprise
#endif  // IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_IOS_H_
