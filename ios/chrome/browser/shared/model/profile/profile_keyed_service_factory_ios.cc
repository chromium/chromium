// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_utils.h"
#include "ios/web/public/browser_state.h"

web::BrowserState* ProfileKeyedServiceFactoryIOS::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetContextToUseForKeyedServiceFactory(context, profile_selection_);
}

bool ProfileKeyedServiceFactoryIOS::ServiceIsCreatedWithBrowserState() const {
  return service_creation_ == ServiceCreation::kCreateWithProfile;
}

bool ProfileKeyedServiceFactoryIOS::ServiceIsNULLWhileTesting() const {
  return testing_creation_ == TestingCreation::kNoServiceForTests;
}

KeyedService* ProfileKeyedServiceFactoryIOS::GetServiceForProfile(
    ProfileIOS* profile,
    bool create) {
  return GetServiceForBrowserState(profile, create);
}

ProfileKeyedServiceFactoryIOS::ProfileKeyedServiceFactoryIOS(
    const char* name,
    ProfileSelection profile_selection,
    ServiceCreation service_creation,
    TestingCreation testing_creation,
    base::trait_helpers::NotATraitTag)
    : BrowserStateKeyedServiceFactory(
          name,
          BrowserStateDependencyManager::GetInstance()),
      profile_selection_(profile_selection),
      service_creation_(service_creation),
      testing_creation_(testing_creation) {}
