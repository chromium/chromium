// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"

#include <memory>
#include <utility>

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeAffiliationServiceFactory*
IOSChromeAffiliationServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAffiliationServiceFactory> instance;
  return instance.get();
}

// static
password_manager::AffiliationService*
IOSChromeAffiliationServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::AffiliationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeAffiliationServiceFactory::IOSChromeAffiliationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AffiliationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

IOSChromeAffiliationServiceFactory::~IOSChromeAffiliationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeAffiliationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state);
  return std::make_unique<password_manager::AffiliationServiceImpl>(
      sync_service, context->GetSharedURLLoaderFactory());
}
