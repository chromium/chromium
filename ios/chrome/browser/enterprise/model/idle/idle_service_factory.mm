// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"

#import "components/enterprise/idle/idle_pref_names.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_idle {

// static
IdleService* IdleServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
IdleService* IdleServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<IdleService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

IdleServiceFactory* IdleServiceFactory::GetInstance() {
  static base::NoDestructor<IdleServiceFactory> instance;
  return instance.get();
}

IdleServiceFactory::IdleServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "IdleService",
          BrowserStateDependencyManager::GetInstance()) {}

IdleServiceFactory::~IdleServiceFactory() = default;

std::unique_ptr<KeyedService> IdleServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<IdleService>(ProfileIOS::FromBrowserState(context));
}

void IdleServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimeDeltaPref(enterprise_idle::prefs::kIdleTimeout,
                                  base::TimeDelta());
  registry->RegisterListPref(enterprise_idle::prefs::kIdleTimeoutActions);
  registry->RegisterTimePref(enterprise_idle::prefs::kLastIdleTimestamp,
                             base::Time());
  registry->RegisterBooleanPref(
      enterprise_idle::prefs::kIdleTimeoutPolicyAppliesToUserOnly, false);
}

}  // namespace enterprise_idle
