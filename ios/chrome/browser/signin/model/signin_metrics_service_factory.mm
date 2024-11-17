// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_metrics_service_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/core/browser/signin_metrics_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
SigninMetricsService* SigninMetricsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SigninMetricsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SigninMetricsServiceFactory* SigninMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SigninMetricsServiceFactory> instance;
  return instance.get();
}

SigninMetricsServiceFactory::SigninMetricsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninMetricsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninMetricsServiceFactory::~SigninMetricsServiceFactory() {}

std::unique_ptr<KeyedService>
SigninMetricsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<SigninMetricsService>(
      *IdentityManagerFactory::GetForProfile(profile), *profile->GetPrefs(),
      GetApplicationContext()->GetActivePrimaryAccountsMetricsRecorder());
}

bool SigninMetricsServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}

void SigninMetricsServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninMetricsService::RegisterProfilePrefs(registry);
}
