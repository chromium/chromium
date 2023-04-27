// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#import "components/password_manager/core/browser/login_database.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/credentials_cleaner_runner_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliations_prefetcher_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::AffiliatedMatchHelper;
using password_manager::AffiliationService;

// static
scoped_refptr<password_manager::PasswordStoreInterface>
IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return nullptr;
  }
  // `browser_state` gets always redirected to a non-Incognito one below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal BrowserState without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord())
    return nullptr;
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
IOSChromeAccountPasswordStoreFactory*
IOSChromeAccountPasswordStoreFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAccountPasswordStoreFactory> instance;
  return instance.get();
}

IOSChromeAccountPasswordStoreFactory::IOSChromeAccountPasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "AccountPasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(CredentialsCleanerRunnerFactory::GetInstance());
}

IOSChromeAccountPasswordStoreFactory::~IOSChromeAccountPasswordStoreFactory() =
    default;

scoped_refptr<RefcountedKeyedService>
IOSChromeAccountPasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(
          browser_state->GetStatePath()));

  auto password_store = base::MakeRefCounted<password_manager::PasswordStore>(
      std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
          std::move(login_db)));

  AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForBrowserState(context);
  std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
      std::make_unique<AffiliatedMatchHelper>(affiliation_service);

  password_store->Init(browser_state->GetPrefs(),
                       std::move(affiliated_match_helper));

  password_manager_util::RemoveUselessCredentials(
      CredentialsCleanerRunnerFactory::GetForBrowserState(browser_state),
      password_store, browser_state->GetPrefs(), base::Minutes(1),
      base::NullCallback());

  IOSChromeAffiliationsPrefetcherFactory::GetForBrowserState(context)
      ->RegisterPasswordStore(password_store.get());
  return password_store;
}

web::BrowserState* IOSChromeAccountPasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool IOSChromeAccountPasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
