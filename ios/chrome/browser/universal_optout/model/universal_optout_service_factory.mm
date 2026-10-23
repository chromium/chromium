// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/universal_optout/model/universal_optout_service_factory.h"

#import <utility>

#import "base/check_deref.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/universal_optout_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/browser_state_utils.h"

namespace universal_optout {

UniversalOptOutServiceFactory::UniversalOptOutServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "UniversalOptOutService",
          ProfileSelection::kRedirectedInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

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
      CHECK_DEREF(IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(
          [](base::WeakPtr<ProfileIOS> weak_profile, bool enabled) {
            if (ProfileIOS* profile = weak_profile.get()) {
              bool effective_enabled =
                  enabled &&
                  base::FeatureList::IsEnabled(
                      features::kUniversalOptOutSettings);
              web::SetUniversalOptOutEnabled(profile, effective_enabled);
              if (profile->HasOffTheRecordProfile()) {
                web::SetUniversalOptOutEnabled(
                    profile->GetOffTheRecordProfile(), effective_enabled);
              }
            }
          },
          profile->AsWeakPtr()));
}

}  // namespace universal_optout
