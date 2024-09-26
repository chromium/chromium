// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/variations/service/google_groups_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
GoogleGroupsManager* GoogleGroupsManagerFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
GoogleGroupsManager* GoogleGroupsManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<GoogleGroupsManager*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
GoogleGroupsManagerFactory*
GoogleGroupsManagerFactory::GetInstance() {
  static base::NoDestructor<GoogleGroupsManagerFactory> instance;
  return instance.get();
}

GoogleGroupsManagerFactory::GoogleGroupsManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "GoogleGroupsManager",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
GoogleGroupsManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<GoogleGroupsManager>(
      *GetApplicationContext()->GetLocalState(), profile->GetProfileName(),
      *profile->GetPrefs());
}

bool GoogleGroupsManagerFactory::ServiceIsCreatedWithBrowserState()
    const {
  return true;
}

bool GoogleGroupsManagerFactory::ServiceIsNULLWhileTesting() const {
  // Many unit tests don't initialize local state prefs, so disable this service
  // in unit tests.
  return true;
}

void GoogleGroupsManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  GoogleGroupsManager::RegisterProfilePrefs(registry);
}
