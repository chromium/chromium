// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

// static
TTCKeyedService* TTCKeyedServiceFactory::GetForProfile(ProfileIOS* profile) {
  if (!IsTTCEnabled()) {
    return nullptr;
  }
  return GetInstance()->GetServiceForProfileAs<TTCKeyedService>(
      profile, /*create=*/true);
}

// static
TTCKeyedServiceFactory* TTCKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<TTCKeyedServiceFactory> instance;
  return instance.get();
}

TTCKeyedServiceFactory::TTCKeyedServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TTCKeyedService") {}

TTCKeyedServiceFactory::~TTCKeyedServiceFactory() = default;

std::unique_ptr<KeyedService> TTCKeyedServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!IsTTCEnabled()) {
    return nullptr;
  }
  return std::make_unique<TTCKeyedService>(profile);
}
