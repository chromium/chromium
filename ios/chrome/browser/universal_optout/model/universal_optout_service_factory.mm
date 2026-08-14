// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/universal_optout/model/universal_optout_service_factory.h"

#import "base/check_deref.h"
#import "base/feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/universal_optout_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace universal_optout {

// static
UniversalOptOutServiceFactory* UniversalOptOutServiceFactory::GetInstance() {
  static base::NoDestructor<UniversalOptOutServiceFactory> instance;
  return instance.get();
}

// static
UniversalOptOutService* UniversalOptOutServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<UniversalOptOutService>(
      profile, /*create=*/true);
}

UniversalOptOutServiceFactory::UniversalOptOutServiceFactory()
    : ProfileKeyedServiceFactoryIOS("UniversalOptOutService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UniversalOptOutServiceFactory::~UniversalOptOutServiceFactory() = default;

std::unique_ptr<KeyedService>
UniversalOptOutServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(features::kUniversalOptOut)) {
    return nullptr;
  }

  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (!variations_service) {
    return nullptr;
  }

  return std::make_unique<UniversalOptOutService>(
      CHECK_DEREF(profile->GetPrefs()), *variations_service,
      CHECK_DEREF(IdentityManagerFactory::GetForProfile(profile)));
}

}  // namespace universal_optout
