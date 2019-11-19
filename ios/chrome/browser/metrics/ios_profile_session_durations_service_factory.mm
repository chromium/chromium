// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <memory>

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"

// static
IOSProfileSessionDurationsService*
IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IOSProfileSessionDurationsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSProfileSessionDurationsServiceFactory::
    ~IOSProfileSessionDurationsServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSProfileSessionDurationsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  return std::make_unique<IOSProfileSessionDurationsService>(sync_service,
                                                             identity_manager);
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
