// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_service_factory.h"

#import "ios/chrome/browser/intelligence/actuation/model/actuation_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ActuationService* ActuationServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ActuationService>(
      profile, /*create=*/true);
}

// static
ActuationServiceFactory* ActuationServiceFactory::GetInstance() {
  static base::NoDestructor<ActuationServiceFactory> instance;
  return instance.get();
}

ActuationServiceFactory::ActuationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ActuationService",
                                    ProfileSelection::kNoInstanceInIncognito) {}

ActuationServiceFactory::~ActuationServiceFactory() {}

std::unique_ptr<KeyedService> ActuationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!IsActuationEnabled()) {
    return nullptr;
  }
  return std::make_unique<ActuationService>(profile);
}
