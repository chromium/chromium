// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/google_groups_updater_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/variations/service/google_groups_updater_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
GoogleGroupsUpdaterService*
GoogleGroupsUpdaterServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  if (!base::FeatureList::IsEnabled(kVariationsGoogleGroupFiltering)) {
    return nullptr;
  }
  return static_cast<GoogleGroupsUpdaterService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
GoogleGroupsUpdaterServiceFactory*
GoogleGroupsUpdaterServiceFactory::GetInstance() {
  static base::NoDestructor<GoogleGroupsUpdaterServiceFactory> instance;
  return instance.get();
}

GoogleGroupsUpdaterServiceFactory::GoogleGroupsUpdaterServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "GoogleGroupsUpdaterService",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
GoogleGroupsUpdaterServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  CHECK(base::FeatureList::IsEnabled(kVariationsGoogleGroupFiltering));
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<GoogleGroupsUpdaterService>(
      *GetApplicationContext()->GetLocalState(),
      browser_state->GetStatePath().BaseName().AsUTF8Unsafe(),
      *browser_state->GetPrefs());
}

bool GoogleGroupsUpdaterServiceFactory::ServiceIsCreatedWithBrowserState()
    const {
  return base::FeatureList::IsEnabled(kVariationsGoogleGroupFiltering);
}

void GoogleGroupsUpdaterServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  if (!base::FeatureList::IsEnabled(kVariationsGoogleGroupFiltering)) {
    return;
  }
  GoogleGroupsUpdaterService::RegisterProfilePrefs(registry);
}
