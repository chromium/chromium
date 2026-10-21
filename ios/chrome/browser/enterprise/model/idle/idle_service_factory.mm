// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"

#import <utility>

#import "components/enterprise/idle/idle_pref_names.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_idle {

IdleServiceFactory::IdleServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "IdleService") {}

std::unique_ptr<KeyedService> IdleServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<IdleService>(profile);
}

void IdleServiceFactory::RegisterProfilePrefs(
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
