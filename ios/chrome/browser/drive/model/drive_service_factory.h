// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace drive {

class DriveService;

// Singleton that owns all instances of DriveService and associates them with
// instances of ProfileIOS.
class DriveServiceFactory final : public ProfileKeyedServiceFactoryIOS {
 public:
  static DriveService* GetForProfile(ProfileIOS* profile);
  static DriveServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<DriveServiceFactory>;

  DriveServiceFactory();
  ~DriveServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_
