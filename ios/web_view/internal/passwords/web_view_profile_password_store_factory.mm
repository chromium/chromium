// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"

#import <memory>
#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/affiliation/password_affiliation_source_adapter.h"
#import "components/password_manager/core/browser/password_store/login_database.h"
#import "components/password_manager/core/browser/password_store/password_store.h"
#import "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "ios/web_view/internal/affiliations/web_view_affiliation_service_factory.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
scoped_refptr<password_manager::PasswordStoreInterface>
WebViewProfilePasswordStoreFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  // |profile| gets always redirected to a non-Incognito profile below, so
  // Incognito & IMPLICIT_ACCESS means that incognito browsing session would
  // result in traces in the normal profile without the user knowing it.
  if (access_type == ServiceAccessType::IMPLICIT_ACCESS &&
      browser_state->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapRefCounted(
      static_cast<password_manager::PasswordStoreInterface*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
WebViewProfilePasswordStoreFactory*
WebViewProfilePasswordStoreFactory::GetInstance() {
  static base::NoDestructor<WebViewProfilePasswordStoreFactory> instance;
  return instance.get();
}

WebViewProfilePasswordStoreFactory::WebViewProfilePasswordStoreFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "ProfilePasswordStore",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewAffiliationServiceFactory::GetInstance());
}

WebViewProfilePasswordStoreFactory::~WebViewProfilePasswordStoreFactory() {}

scoped_refptr<RefcountedKeyedService>
WebViewProfilePasswordStoreFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  PrefService* prefs = browser_state->GetPrefs();
  std::unique_ptr<password_manager::LoginDatabase> login_db(
      password_manager::CreateLoginDatabase(password_manager::kProfileStore,
                                            context->GetStatePath(), prefs));
  scoped_refptr<password_manager::PasswordStore> store =
      new password_manager::PasswordStore(
          std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
              std::move(login_db),
              syncer::WipeModelUponSyncDisabledBehavior::kNever, prefs,
              ApplicationContext::GetInstance()->GetOSCryptAsync()));
  affiliations::AffiliationService* affiliation_service =
      WebViewAffiliationServiceFactory::GetForBrowserState(browser_state);
  store->Init(std::make_unique<password_manager::AffiliatedMatchHelper>(
      affiliation_service));
  auto password_affiliation_adapter =
      std::make_unique<password_manager::PasswordAffiliationSourceAdapter>();
  password_affiliation_adapter->RegisterPasswordStore(store.get());
  affiliation_service->RegisterSource(std::move(password_affiliation_adapter));
  return store;
}

web::BrowserState* WebViewProfilePasswordStoreFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

bool WebViewProfilePasswordStoreFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios_web_view
