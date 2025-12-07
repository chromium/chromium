// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace enterprise {

class ProfileIdService;

class ProfileIdServiceFactoryIOS : public ProfileKeyedServiceFactoryIOS {
 public:
  static ProfileIdService* GetForProfile(ProfileIOS* profile);

  // Returns the ProfileIdService for the given profile and nullptr for the
  // Incognito profile.
  static ProfileIdServiceFactoryIOS* GetInstance();

  // Returns the default factory used to build ProfileIdService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<ProfileIdServiceFactoryIOS>;

  ProfileIdServiceFactoryIOS();
  ~ProfileIdServiceFactoryIOS() override;

  // ProfileKeyedServiceFactoryIOS overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace enterprise
#endif  // IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_SERVICE_FACTORY_IOS_H_
