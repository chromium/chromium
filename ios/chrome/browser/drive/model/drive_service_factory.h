// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace drive {

class DriveService;

// Singleton that owns all instances of DriveService and associates them with
// instances of ProfileIOS.
class DriveServiceFactory final : public BrowserStateKeyedServiceFactory {
 public:
  static DriveService* GetForProfile(ProfileIOS* profile);
  // Deprecated: use GetForProfile(...).
  static DriveService* GetForBrowserState(ProfileIOS* profile);
  static DriveServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<DriveServiceFactory>;

  DriveServiceFactory();
  ~DriveServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_FACTORY_H_
