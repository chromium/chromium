// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_reuse_manager_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

// static
IOSChromePasswordReuseManagerFactory*
IOSChromePasswordReuseManagerFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordReuseManagerFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordReuseManager*
IOSChromePasswordReuseManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordReuseDetectionEnabled)) {
    return nullptr;
  }

  return static_cast<password_manager::PasswordReuseManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromePasswordReuseManagerFactory::IOSChromePasswordReuseManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordReuseManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSChromePasswordStoreFactory::GetInstance());
}

IOSChromePasswordReuseManagerFactory::~IOSChromePasswordReuseManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromePasswordReuseManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(base::FeatureList::IsEnabled(
      password_manager::features::kPasswordReuseDetectionEnabled));
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<password_manager::PasswordReuseManager> reuse_manager =
      std::make_unique<password_manager::PasswordReuseManagerImpl>();

  reuse_manager->Init(browser_state->GetPrefs(),
                      IOSChromePasswordStoreFactory::GetForBrowserState(
                          browser_state, ServiceAccessType::EXPLICIT_ACCESS)
                          .get(),
                      IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
                          browser_state, ServiceAccessType::EXPLICIT_ACCESS)
                          .get());
  return reuse_manager;
}

web::BrowserState* IOSChromePasswordReuseManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
