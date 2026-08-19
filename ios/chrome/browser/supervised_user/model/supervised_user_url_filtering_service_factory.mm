// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filtering_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#import "components/supervised_user/core/browser/family_link_settings_service.h"
#import "components/supervised_user/core/browser/family_link_url_filter.h"
#import "components/supervised_user/core/browser/supervised_user_url_checker_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_platform_delegate.h"
#import "ios/chrome/common/channel_info.h"

namespace supervised_user {

namespace {

// Implementation of the supervised user filter delegate interface.
class FilterDelegateImpl : public FamilyLinkUrlFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override { return false; }
};

}  // namespace

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<SupervisedUserUrlFilteringService>(
          profile, /*create=*/true);
}

// static
SupervisedUserUrlFilteringServiceFactory*
SupervisedUserUrlFilteringServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserUrlFilteringServiceFactory> instance;
  return instance.get();
}

SupervisedUserUrlFilteringServiceFactory::
    SupervisedUserUrlFilteringServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SupervisedUserUrlFilteringService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(FamilyLinkSettingsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserUrlFilteringServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // TODO(crbug.com/297313665): Use command line hooks to enable substitution of
  // prod components with test components (with extended interfaces).

  SupervisedUserServicePlatformDelegate platform_delegate(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      GetApplicationContext()->GetSharedURLLoaderFactory();

  return std::make_unique<SupervisedUserUrlFilteringService>(
      std::make_unique<FamilyLinkUrlFilter>(
          CHECK_DEREF(FamilyLinkSettingsServiceFactory::GetForProfile(profile)),
          CHECK_DEREF(profile->GetPrefs()),
          std::make_unique<FilterDelegateImpl>(),
          std::make_unique<SupervisedUserUrlCheckerClient>(
              identity_manager, url_loader_factory,
              CHECK_DEREF(profile->GetPrefs()),
              platform_delegate.GetCountryCode(),
              platform_delegate.GetChannel())),
      std::make_unique<DeviceParentalControlsUrlFilter>(
          GetApplicationContext()->GetDeviceParentalControls(),
          std::make_unique<SupervisedUserUrlCheckerClient>(
              url_loader_factory, platform_delegate.GetCountryCode(),
              platform_delegate.GetChannel())));
}
}  // namespace supervised_user
