// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
scoped_refptr<password_manager::PasswordStoreInterface>
WebViewAccountPasswordStoreFactory::GetForBrowserState(
    WebViewBrowserState* browser_state,
    ServiceAccessType access_type) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return nullptr;
  }

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
          BrowserStateDependencyManager::GetInstance()) {}

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
              std::move(login_db)));
  ps->Init(browser_state->GetPrefs(), /*affiliated_match_helper=*/nullptr);

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
