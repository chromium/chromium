// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_

#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace drive {

// Singleton that owns all instances of DriveService and associates them with
// instances of ProfileIOS.
class DriveServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<DriveServiceFactory,
                                                DriveService> {
 public:
  explicit DriveServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_
