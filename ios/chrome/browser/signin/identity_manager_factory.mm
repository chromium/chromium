// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/identity_manager_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_client.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_manager_builder.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/device_accounts_provider_impl.h"
#import "ios/chrome/browser/signin/identity_manager_factory_observer.h"
#import "ios/chrome/browser/signin/signin_client_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void IdentityManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  signin::IdentityManager::RegisterProfilePrefs(registry);
}

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "IdentityManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(SigninClientFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
signin::IdentityManager* IdentityManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<signin::IdentityManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
signin::IdentityManager* IdentityManagerFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<signin::IdentityManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  static base::NoDestructor<IdentityManagerFactory> instance;
  return instance.get();
}

void IdentityManagerFactory::AddObserver(
    IdentityManagerFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManagerFactory::RemoveObserver(
    IdentityManagerFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

std::unique_ptr<KeyedService> IdentityManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  signin::IdentityManagerBuildParams params;
  params.account_consistency = signin::AccountConsistencyMethod::kMirror;
  params.device_accounts_provider =
      std::make_unique<DeviceAccountsProviderImpl>(
          ChromeAccountManagerServiceFactory::GetForBrowserState(
              browser_state));
  params.image_decoder = image_fetcher::CreateIOSImageDecoder();
  params.local_state = GetApplicationContext()->GetLocalState();
  params.pref_service = browser_state->GetPrefs();
  params.profile_path = base::FilePath();
  params.signin_client = SigninClientFactory::GetForBrowserState(browser_state);

  std::unique_ptr<signin::IdentityManager> identity_manager =
      signin::BuildIdentityManager(&params);

  for (auto& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager;
}
