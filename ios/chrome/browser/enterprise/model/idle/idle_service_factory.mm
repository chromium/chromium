// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"

#import "components/enterprise/idle/idle_pref_names.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace enterprise_idle {

IdleService* IdleServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<IdleService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  return std::make_unique<IdleService>(
      ChromeBrowserState::FromBrowserState(context));
}

void IdleServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimeDeltaPref(enterprise_idle::prefs::kIdleTimeout,
                                  base::TimeDelta());
  registry->RegisterListPref(enterprise_idle::prefs::kIdleTimeoutActions);
  registry->RegisterTimePref(enterprise_idle::prefs::kLastIdleTimestamp,
                             base::Time());
}

}  // namespace enterprise_idle
