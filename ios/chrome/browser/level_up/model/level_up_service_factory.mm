// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
LevelUpService* LevelUpServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<LevelUpService>(profile,
                                                               /*create=*/true);
}

// static
LevelUpServiceFactory* LevelUpServiceFactory::GetInstance() {
  static base::NoDestructor<LevelUpServiceFactory> instance;
  return instance.get();
}

LevelUpServiceFactory::LevelUpServiceFactory()
    : ProfileKeyedServiceFactoryIOS("LevelUpService") {}

LevelUpServiceFactory::~LevelUpServiceFactory() {}

std::unique_ptr<KeyedService> LevelUpServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<LevelUpService>();
}
