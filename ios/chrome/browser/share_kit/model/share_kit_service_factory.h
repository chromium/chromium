// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ShareKitService;

// Singleton that owns all instances of ShareKitService and associates them with
// instances of ProfileIOS.
class ShareKitServiceFactory final : public ProfileKeyedServiceFactoryIOS {
 public:
  static ShareKitService* GetForProfile(ProfileIOS* profile);
  static ShareKitServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ShareKitServiceFactory>;

  ShareKitServiceFactory();
  ~ShareKitServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_FACTORY_H_
