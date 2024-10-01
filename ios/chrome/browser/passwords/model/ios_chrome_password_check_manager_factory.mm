// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"

#import "base/memory/ref_counted.h"
#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"

// static
IOSChromePasswordCheckManagerFactory*
IOSChromePasswordCheckManagerFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordCheckManagerFactory> instance;
  return instance.get();
}

// static
scoped_refptr<IOSChromePasswordCheckManager>
IOSChromePasswordCheckManagerFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
scoped_refptr<IOSChromePasswordCheckManager>
IOSChromePasswordCheckManagerFactory::GetForProfile(ProfileIOS* profile) {
  return base::WrapRefCounted(static_cast<IOSChromePasswordCheckManager*>(
      GetInstance()->GetServiceForBrowserState(profile, true).get()));
}

IOSChromePasswordCheckManagerFactory::IOSChromePasswordCheckManagerFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "PasswordCheckManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
  DependsOn(IOSChromeBulkLeakCheckServiceFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
  DependsOn(IOSPasskeyModelFactory::GetInstance());
}

IOSChromePasswordCheckManagerFactory::~IOSChromePasswordCheckManagerFactory() =
    default;

scoped_refptr<RefcountedKeyedService>
IOSChromePasswordCheckManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return base::MakeRefCounted<IOSChromePasswordCheckManager>(
      profile->GetPrefs(),
      IOSChromeBulkLeakCheckServiceFactory::GetForProfile(profile),
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          IOSChromeAffiliationServiceFactory::GetForProfile(profile),
          IOSChromeProfilePasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          IOSChromeAccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          IOSPasskeyModelFactory::GetForProfile(profile)));
}
