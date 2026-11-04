// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_url_filtering_service_factory.h"

#import <memory>
#import <string>
#import <utility>

#import "base/check_deref.h"
#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#import "components/supervised_user/core/browser/family_link_settings_service.h"
#import "components/supervised_user/core/browser/family_link_url_filter.h"
#import "components/supervised_user/core/browser/supervised_user_url_checker_client.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/family_link_settings_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace supervised_user {

namespace {

// Implementation of the supervised user filter delegate interface.
class FilterDelegateImpl : public FamilyLinkUrlFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override { return false; }
};

// Returns the country code reported by the variations service, or an empty
// string if it is unavailable.
std::string GetSupervisedUserCountryCode() {
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

}  // namespace

SupervisedUserUrlFilteringServiceFactory::
    SupervisedUserUrlFilteringServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SupervisedUserUrlFilteringService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(FamilyLinkSettingsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SupervisedUserUrlFilteringServiceFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  // TODO(crbug.com/297313665): Use command line hooks to enable substitution of
  // prod components with test components (with extended interfaces).

  const std::string country_code = GetSupervisedUserCountryCode();
  const version_info::Channel channel = ::GetChannel();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      GetApplicationContext()->GetSharedURLLoaderFactory();

  return std::make_unique<SupervisedUserUrlFilteringService>(
      std::make_unique<FamilyLinkUrlFilter>(
          CHECK_DEREF(FamilyLinkSettingsServiceFactory::GetForProfile(profile)),
          CHECK_DEREF(profile.GetPrefs()),
          std::make_unique<FilterDelegateImpl>(),
          std::make_unique<SupervisedUserUrlCheckerClient>(
              identity_manager, url_loader_factory,
              CHECK_DEREF(profile.GetPrefs()), country_code, channel)),
      std::make_unique<DeviceParentalControlsUrlFilter>(
          GetApplicationContext()->GetDeviceParentalControls(),
          std::make_unique<SupervisedUserUrlCheckerClient>(
              url_loader_factory, country_code, channel)));
}
}  // namespace supervised_user
