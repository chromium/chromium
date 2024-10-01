// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
IOSChromeBulkLeakCheckServiceFactory*
IOSChromeBulkLeakCheckServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeBulkLeakCheckServiceFactory> instance;
  return instance.get();
}

// static
password_manager::BulkLeakCheckServiceInterface*
IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
password_manager::BulkLeakCheckServiceInterface*
IOSChromeBulkLeakCheckServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<password_manager::BulkLeakCheckServiceInterface*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

IOSChromeBulkLeakCheckServiceFactory::IOSChromeBulkLeakCheckServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordBulkLeakCheck",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSChromeBulkLeakCheckServiceFactory::~IOSChromeBulkLeakCheckServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeBulkLeakCheckServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
      test_bulk_leak_check_service =
          tests_hook::GetOverriddenBulkLeakCheckService();
  if (test_bulk_leak_check_service) {
    return test_bulk_leak_check_service;
  }
  return std::make_unique<password_manager::BulkLeakCheckService>(
      IdentityManagerFactory::GetForProfile(profile),
      context->GetSharedURLLoaderFactory());
}
