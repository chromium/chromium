// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"

#import <string>
#import <string_view>
#import <utility>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "build/branding_buildflags.h"
#import "components/contextual_tasks/public/features.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"
#import "net/http/structured_headers.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Serializes the `Sec-CH-UA-Full-Version-List` structured header value (RFC
// 8941) using the client's full version number.
// TODO(crbug.com/578470030): Replace this local helper with a //ios/web API
// (e.g. on web::WebClient) for User-Agent metadata / Client Hints.
std::string GetFullVersionList() {
  const std::string_view full_version = version_info::GetVersionNumber();
  auto make_brand_item = [full_version](std::string_view brand) {
    net::structured_headers::Parameters params;
    params.emplace_back(
        "v", net::structured_headers::Item(
                 net::structured_headers::Item::string, full_version));
    return net::structured_headers::ParameterizedMember(
        net::structured_headers::Item(net::structured_headers::Item::string,
                                      brand),
        std::move(params));
  };

  net::structured_headers::List brand_version_header;
  brand_version_header.push_back(make_brand_item("Chromium"));
#if !BUILDFLAG(CHROMIUM_BRANDING)
  brand_version_header.push_back(
      make_brand_item(version_info::GetProductName()));
#endif
  return net::structured_headers::SerializeList(brand_version_header)
      .value_or("");
}

AimEligibilityService::Configuration CreateConfiguration(
    bool is_off_the_record) {
  AimEligibilityService::Configuration config;
  config.is_off_the_record = is_off_the_record;
  // TODO(crbug.com/578470030): Query full version list via a //ios/web API
  // once available.
  config.full_version_list = GetFullVersionList();
  if (!base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    return config;
  }

  config.user_agent_with_cobrowse_suffix = base::StrCat(
      {web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE), " ",
       contextual_tasks::GetContextualTasksUserAgentSuffix()});
  config.search_capabilities_version =
      contextual_tasks::GetContextualTasksSearchCapabilitiesVersion();

  return config;
}

std::unique_ptr<KeyedService> BuildAimEligibilityService(ProfileIOS* profile) {
  if (auto service = tests_hook::CreateAimEligibilityService(profile)) {
    return service;
  }
  return std::make_unique<IOSChromeAimEligibilityService>(
      profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile),
      CreateConfiguration(profile->IsOffTheRecord()));
}

}  // namespace

// static
IOSChromeAimEligibilityServiceFactory::TestingFactory
IOSChromeAimEligibilityServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildAimEligibilityService);
}

IOSChromeAimEligibilityServiceFactory::IOSChromeAimEligibilityServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AimEligibilityService",
          IsAIMEligibilityServiceStartWithProfileEnabled()
              ? ServiceCreation::kCreateWithProfile
              : ServiceCreation::kDefault,
          ProfileSelection::kOwnInstanceInIncognito,
          IsAIMEligibilityServiceStartWithProfileEnabled()
              ? TestingCreation::kNoServiceForTests
              : TestingCreation::kDefault) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeAimEligibilityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildAimEligibilityService(profile);
}
