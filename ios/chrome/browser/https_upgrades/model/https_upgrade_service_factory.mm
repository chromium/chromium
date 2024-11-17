// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

// static
HttpsUpgradeService* HttpsUpgradeServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<HttpsUpgradeService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
HttpsUpgradeServiceFactory* HttpsUpgradeServiceFactory::GetInstance() {
  static base::NoDestructor<HttpsUpgradeServiceFactory> instance;
  return instance.get();
}

HttpsUpgradeServiceFactory::HttpsUpgradeServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "HttpsUpgradeService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

HttpsUpgradeServiceFactory::~HttpsUpgradeServiceFactory() {}

std::unique_ptr<KeyedService>
HttpsUpgradeServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<HttpsUpgradeServiceImpl>(
      ProfileIOS::FromBrowserState(context));
}

web::BrowserState* HttpsUpgradeServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool HttpsUpgradeServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}
