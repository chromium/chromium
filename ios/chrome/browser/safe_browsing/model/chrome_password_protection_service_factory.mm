// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service_factory.h"

#import <utility>

#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

ChromePasswordProtectionServiceFactory::ChromePasswordProtectionServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ChromePasswordProtectionService",
          ProfileSelection::kOwnInstanceInIncognito,
          TestingCreation::kNoServiceForTests) {
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
    ProfileIOS* profile) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }
  return std::make_unique<ChromePasswordProtectionService>(
      safe_browsing_service, profile,
      ios::HistoryServiceFactory::GetForProfile(profile),
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile));
}
