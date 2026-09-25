// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ORIGIN_GATING_MODEL_ORIGIN_GATING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ORIGIN_GATING_MODEL_ORIGIN_GATING_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace origin_gating {

class OriginGatingService;

// Singleton that owns all OriginGatingServices and associates them with
// ProfileIOS.
class OriginGatingServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static OriginGatingService* GetForProfile(ProfileIOS* profile);
  static OriginGatingServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<OriginGatingServiceFactory>;

  OriginGatingServiceFactory();
  ~OriginGatingServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace origin_gating

#endif  // IOS_CHROME_BROWSER_ORIGIN_GATING_MODEL_ORIGIN_GATING_SERVICE_FACTORY_H_
