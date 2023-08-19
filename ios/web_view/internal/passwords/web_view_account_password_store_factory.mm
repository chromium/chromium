// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/login_database.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/prefs/pref_service.h"
#import "ios/web_view/internal/passwords/web_view_affiliation_service_factory.h"
#import "ios/web_view/internal/passwords/web_view_affiliations_prefetcher_factory.h"

namespace ios_web_view {

// static
scoped_refptr<password_manager::PasswordStoreInterface>
WebViewAccountPasswordStoreFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  CHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  // |browser_state| always gets redirected to a the recording version in
  // |GetBrowserStateToUse|.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord()) {
    return nullptr;
  }

  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
WebViewAccountPasswordStoreFactory*
WebViewAccountPasswordStoreFactory::GetInstance() {
  static base::NoDestructor<WebViewAccountPasswordStoreFactory> instance;
  return instance.get();
}

WebViewAccountPasswordStoreFactory::WebViewAccountPasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "AccountPasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewAffiliationServiceFactory::GetInstance());
  DependsOn(WebViewAffiliationsPrefetcherFactory::GetInstance());
}

WebViewAccountPasswordStoreFactory::~WebViewAccountPasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
WebViewAccountPasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabaseForAccountStorage(
          browser_state->GetStatePath()));

  scoped_refptr<password_manager::PasswordStore> ps =
      new password_manager::PasswordStore(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db),
              syncer::WipeModelUponSyncDisabledBehavior::kAlways));

  password_manager::AffiliationService* affiliation_service =
      WebViewAffiliationServiceFactory::GetForBrowserState(context);
  std::unique_ptr<password_manager::AffiliatedMatchHelper>
      affiliated_match_helper =
          std::make_unique<password_manager::AffiliatedMatchHelper>(
              affiliation_service);

  ps->Init(browser_state->GetPrefs(), std::move(affiliated_match_helper));

  WebViewAffiliationsPrefetcherFactory::GetForBrowserState(context)
      ->RegisterPasswordStore(ps.get());

  return ps;
}

web::BrowserState* WebViewAccountPasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

bool WebViewAccountPasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios_web_view
