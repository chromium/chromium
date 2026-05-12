// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
BackendPromoService* BackendPromoServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<BackendPromoService>(
      profile, /*create=*/true);
}

// static
BackendPromoServiceFactory* BackendPromoServiceFactory::GetInstance() {
  static base::NoDestructor<BackendPromoServiceFactory> instance;
  return instance.get();
}

BackendPromoServiceFactory::BackendPromoServiceFactory()
    : ProfileKeyedServiceFactoryIOS("BackendPromoService") {}

BackendPromoServiceFactory::~BackendPromoServiceFactory() {}

std::unique_ptr<KeyedService>
BackendPromoServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return ios::provider::CreateBackendPromoService();
}
