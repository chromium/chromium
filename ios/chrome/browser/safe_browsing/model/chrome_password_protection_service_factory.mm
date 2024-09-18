// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
ChromePasswordProtectionService*
ChromePasswordProtectionServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<ChromePasswordProtectionService*>(
      GetInstance()->GetServiceForBrowserState(profile, /*create=*/true));
}

// static
ChromePasswordProtectionService*
ChromePasswordProtectionServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
ChromePasswordProtectionServiceFactory*
ChromePasswordProtectionServiceFactory::GetInstance() {
  static base::NoDestructor<ChromePasswordProtectionServiceFactory> instance;
  return instance.get();
}

ChromePasswordProtectionServiceFactory::ChromePasswordProtectionServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ChromePasswordProtectionService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSUserEventServiceFactory::GetInstance());
  DependsOn(SafeBrowsingMetricsCollectorFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ChromePasswordProtectionServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  return std::make_unique<ChromePasswordProtectionService>(
      safe_browsing_service, profile,
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile));
}

bool ChromePasswordProtectionServiceFactory::ServiceIsCreatedWithBrowserState()
    const {
  return false;
}

web::BrowserState* ChromePasswordProtectionServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool ChromePasswordProtectionServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
