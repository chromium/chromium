// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

#import "base/check_deref.h"
#import "base/no_destructor.h"
#import "base/version_info/channel.h"
#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_platform_delegate.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "url/gurl.h"

namespace {

// Implementation of the supervised user filter delegate interface.
class FilterDelegateImpl
    : public supervised_user::SupervisedUserURLFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override { return false; }
};

}  // namespace

namespace supervised_user {
bool ShouldShowFirstTimeBanner(ProfileIOS* profile) {
  // We perceive the current user as an existing one if there is an existing
  // preference file, except on first run, because the installer may create a
  // preference file.
  // This implementation mimics `Profile::IsNewProfile()`, used in native.
  if (FirstRun::IsChromeFirstRun()) {
    return false;
  }
  return profile->GetPrefs()->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}
}  // namespace supervised_user

// static
supervised_user::SupervisedUserService*
SupervisedUserServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<supervised_user::SupervisedUserService>(
          profile, /*create=*/true);
}

// static
SupervisedUserServiceFactory* SupervisedUserServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserServiceFactory> instance;
  return instance.get();
}

SupervisedUserServiceFactory::SupervisedUserServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SupervisedUserService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<supervised_user::SupervisedUserService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(), CHECK_DEREF(profile->GetPrefs()),
      CHECK_DEREF(SupervisedUserSettingsServiceFactory::GetForProfile(profile)),
      &CHECK_DEREF(SyncServiceFactory::GetForProfile(profile)),
      std::make_unique<FilterDelegateImpl>(),
      std::make_unique<SupervisedUserServicePlatformDelegate>(profile),
      supervised_user::ShouldShowFirstTimeBanner(profile));
}
