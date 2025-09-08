// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class MiniMapService;

// Factory for the MiniMapService that will observe the profile scoped settings.
class MiniMapServiceFactory final : public ProfileKeyedServiceFactoryIOS {
 public:
  static MiniMapService* GetForProfile(ProfileIOS* profile);
  static MiniMapServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<MiniMapServiceFactory>;

  MiniMapServiceFactory();
  ~MiniMapServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_FACTORY_H_
