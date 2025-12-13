// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_metrics_service_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/core/browser/signin_metrics_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
SigninMetricsService* SigninMetricsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SigninMetricsService>(
      profile, /*create=*/true);
}

// static
SigninMetricsServiceFactory* SigninMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SigninMetricsServiceFactory> instance;
  return instance.get();
}

SigninMetricsServiceFactory::SigninMetricsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SigninMetricsService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninMetricsServiceFactory::~SigninMetricsServiceFactory() {}

std::unique_ptr<KeyedService>
SigninMetricsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<SigninMetricsService>(
      *IdentityManagerFactory::GetForProfile(profile), *profile->GetPrefs(),
      GetApplicationContext()->GetActivePrimaryAccountsMetricsRecorder());
}

void SigninMetricsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninMetricsService::RegisterProfilePrefs(registry);
}
