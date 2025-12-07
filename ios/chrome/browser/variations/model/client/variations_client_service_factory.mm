// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"

// static
VariationsClientService* VariationsClientServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<VariationsClientService>(
      profile, /*create=*/true);
}

// static
VariationsClientServiceFactory* VariationsClientServiceFactory::GetInstance() {
  static base::NoDestructor<VariationsClientServiceFactory> instance;
  return instance.get();
}

VariationsClientServiceFactory::VariationsClientServiceFactory()
    : ProfileKeyedServiceFactoryIOS("VariationsClientService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {}

VariationsClientServiceFactory::~VariationsClientServiceFactory() = default;

std::unique_ptr<KeyedService>
VariationsClientServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<VariationsClientService>(profile);
}
