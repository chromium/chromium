// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/kids_chrome_management_client_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "url/gurl.h"

namespace {

// Implementation of the supervised user filter delegate interface.
class FilterDelegateImpl
    : public supervised_user::SupervisedUserURLFilter::Delegate {
 public:
  std::string GetCountryCode() override {
    std::string country;
    variations::VariationsService* variations_service =
        GetApplicationContext()->GetVariationsService();
    if (variations_service) {
      country = variations_service->GetStoredPermanentCountry();
      if (country.empty()) {
        country = variations_service->GetLatestCountry();
      }
    }
    return country;
  }
};

}  // namespace

namespace supervised_user {
bool ShouldShowFirstTimeBanner(ChromeBrowserState* browser_state) {
  // We perceive the current user as an existing one if there is an existing
  // preference file, except on first run, because the installer may create a
  // preference file.
  // This implementation mimics `Profile::IsNewProfile()`, used in native.
  if (FirstRun::IsChromeFirstRun()) {
    return false;
  }
  return browser_state->GetPrefs()->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}
}  // namespace supervised_user

// static
supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<supervised_user::SupervisedUserService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
SupervisedUserServiceFactory* SupervisedUserServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserServiceFactory> instance;
  return instance.get();
}

SupervisedUserServiceFactory::SupervisedUserServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SupervisedUserService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(KidsChromeManagementClientFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(browser_state);
  CHECK(settings_service);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  CHECK(sync_service);
  PrefService* user_prefs = browser_state->GetPrefs();
  CHECK(user_prefs);

  return std::make_unique<supervised_user::SupervisedUserService>(
      IdentityManagerFactory::GetForBrowserState(browser_state),
      KidsChromeManagementClientFactory::GetForBrowserState(browser_state),
      *user_prefs, *settings_service, *sync_service,
      // iOS does not support extensions, check_webstore_url_callback returns
      // false.
      base::BindRepeating([](const GURL& url) { return false; }),
      std::make_unique<FilterDelegateImpl>(),
      supervised_user::ShouldShowFirstTimeBanner(browser_state));
}
