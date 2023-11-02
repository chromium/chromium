// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_setup_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"

// static
SyncSetupService* SyncSetupServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SyncSetupService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SyncSetupService* SyncSetupServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<SyncSetupService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
SyncSetupServiceFactory* SyncSetupServiceFactory::GetInstance() {
  static base::NoDestructor<SyncSetupServiceFactory> instance;
  return instance.get();
}

SyncSetupServiceFactory::SyncSetupServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncSetupService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SyncServiceFactory::GetInstance());
}

SyncSetupServiceFactory::~SyncSetupServiceFactory() {}

std::unique_ptr<KeyedService> SyncSetupServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupService>(
      SyncServiceFactory::GetForBrowserState(browser_state));
}
