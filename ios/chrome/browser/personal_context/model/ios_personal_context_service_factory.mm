// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_service_factory.h"

#import "base/feature_list.h"
#import "components/personal_context/core/personal_context_features.h"
#import "components/personal_context/core/personal_context_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using personal_context::PersonalContextService;
using personal_context::PersonalContextServiceImpl;

// static
PersonalContextService* IOSPersonalContextServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PersonalContextService>(profile,
                                                                       true);
}

// static
IOSPersonalContextServiceFactory*
IOSPersonalContextServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPersonalContextServiceFactory> instance;
  return instance.get();
}

IOSPersonalContextServiceFactory::IOSPersonalContextServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PersonalContextService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSPersonalContextServiceFactory::~IOSPersonalContextServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSPersonalContextServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          personal_context::features::kPersonalContext)) {
    return nullptr;
  }

  return std::make_unique<PersonalContextServiceImpl>(
      profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile));
}
