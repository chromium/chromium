// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
IOSProfileSessionDurationsService*
IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
IOSProfileSessionDurationsService*
IOSProfileSessionDurationsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<IOSProfileSessionDurationsService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
IOSProfileSessionDurationsServiceFactory*
IOSProfileSessionDurationsServiceFactory::GetInstance() {
  static base::NoDestructor<IOSProfileSessionDurationsServiceFactory> instance;
  return instance.get();
}

IOSProfileSessionDurationsServiceFactory::
    IOSProfileSessionDurationsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "IOSProfileSessionDurationsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSProfileSessionDurationsServiceFactory::
    ~IOSProfileSessionDurationsServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSProfileSessionDurationsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<IOSProfileSessionDurationsService>(
      sync_service, profile->GetPrefs(), identity_manager);
}

web::BrowserState*
IOSProfileSessionDurationsServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Session time in incognito is counted towards the session time in the
  // regular profile. That means that for a user that is signed in and syncing
  // in their regular profile and that is browsing in incognito profile,
  // Chromium will record the session time as being signed in and syncing.
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSProfileSessionDurationsServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}
