// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/internal/identity_manager/account_tracker_service.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"
#import "components/signin/public/base/signin_client.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_manager_builder.h"
#import "components/sync/base/features.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_factory_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/device_accounts_provider_impl.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory_observer.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"

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
  params.account_capabilities_fetcher_factory =
      std::make_unique<ios::AccountCapabilitiesFetcherFactoryIOS>(
          ChromeAccountManagerServiceFactory::GetForBrowserState(
              browser_state));
  params.image_decoder = image_fetcher::CreateIOSImageDecoder();
  params.local_state = GetApplicationContext()->GetLocalState();
  params.pref_service = browser_state->GetPrefs();
  params.profile_path = base::FilePath();
  params.signin_client = SigninClientFactory::GetForBrowserState(browser_state);
  params.account_tracker_service = std::make_unique<AccountTrackerService>();
  params.account_tracker_service->Initialize(params.pref_service,
                                             params.profile_path);
  params.should_verify_scope_access =
      !base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos);

  std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate =
      std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
          params.signin_client,
          std::make_unique<DeviceAccountsProviderImpl>(
              ChromeAccountManagerServiceFactory::GetForBrowserState(
                  browser_state)),
          params.account_tracker_service.get());
  params.token_service = tests_hook::GetOverriddenTokenService(
      params.pref_service, std::move(delegate));

  std::unique_ptr<signin::IdentityManager> identity_manager =
      signin::BuildIdentityManager(&params);

  for (auto& observer : observer_list_)
    observer.IdentityManagerCreated(identity_manager.get());

  return identity_manager;
}
