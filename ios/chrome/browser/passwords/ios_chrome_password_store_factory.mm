// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/callback_helpers.h"
#import "base/command_line.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#import "components/password_manager/core/browser/login_database.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/passwords/credentials_cleaner_runner_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliations_prefetcher_factory.h"
#import "ios/chrome/browser/passwords/ios_password_store_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::AffiliatedMatchHelper;
using password_manager::AffiliationService;

// static
scoped_refptr<password_manager::PasswordStoreInterface>
IOSChromePasswordStoreFactory::GetForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  // `profile` gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord())
    return nullptr;
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
IOSChromePasswordStoreFactory* IOSChromePasswordStoreFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordStoreFactory> instance;
  return instance.get();
}

IOSChromePasswordStoreFactory::IOSChromePasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "PasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
  DependsOn(IOSChromeAffiliationsPrefetcherFactory::GetInstance());
}

IOSChromePasswordStoreFactory::~IOSChromePasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
IOSChromePasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForProfileStorage(
          context->GetStatePath()));

  scoped_refptr<password_manager::PasswordStore> store =
      base::MakeRefCounted<password_manager::PasswordStore>(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db)));

  AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForBrowserState(context);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  store->Init(ChromeBrowserState::FromBrowserState(context)->GetPrefs(),
              std::move(affiliated_match_helper));

  password_manager_util::RemoveUselessCredentials(
      CredentialsCleanerRunnerFactory::GetForBrowserState(context), store,
      ChromeBrowserState::FromBrowserState(context)->GetPrefs(),
      base::Seconds(60), base::NullCallback());
  if (!context->IsOffTheRecord()) {
    DelayReportingPasswordStoreMetrics(
        ChromeBrowserState::FromBrowserState(context));
  }

  IOSChromeAffiliationsPrefetcherFactory::GetForBrowserState(context)
      ->RegisterPasswordStore(store.get());
  return store;
}

web::BrowserState* IOSChromePasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSChromePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
