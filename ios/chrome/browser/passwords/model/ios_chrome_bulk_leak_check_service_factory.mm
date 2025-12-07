// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeBulkLeakCheckServiceFactory*
IOSChromeBulkLeakCheckServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeBulkLeakCheckServiceFactory> instance;
  return instance.get();
}

// static
password_manager::BulkLeakCheckServiceInterface*
IOSChromeBulkLeakCheckServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<password_manager::BulkLeakCheckServiceInterface>(
          profile, /*create=*/true);
}

IOSChromeBulkLeakCheckServiceFactory::IOSChromeBulkLeakCheckServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PasswordBulkLeakCheck") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSChromeBulkLeakCheckServiceFactory::~IOSChromeBulkLeakCheckServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeBulkLeakCheckServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
      test_bulk_leak_check_service =
          tests_hook::GetOverriddenBulkLeakCheckService();
  if (test_bulk_leak_check_service) {
    return test_bulk_leak_check_service;
  }
  return std::make_unique<password_manager::BulkLeakCheckService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory());
}
