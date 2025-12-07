// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/chrome_enterprise_url_lookup_service_factory.h"

#import <memory>

#import "components/enterprise/connectors/core/common.h"
#import "components/enterprise/connectors/core/content_area_user_provider.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"
#import "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/management_service_ios.h"
#import "ios/chrome/browser/policy/model/management_service_ios_factory.h"
#import "ios/chrome/browser/safe_browsing/model/user_population_helper.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Returns true if the policy command line switch can be used.
bool IsCommandLineSwitchEnabled() {
  BrowserPolicyConnectorIOS* browser_policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  return browser_policy_connector &&
         browser_policy_connector->IsCommandLineSwitchSupported();
}

// Returns true if the profile and browser are managed by the same enterprise
// costumer.
bool IsProfileAffiliated(ProfileIOS* profile) {
  return policy::IsAffiliated(
      enterprise_connectors::GetUserAffiliationIds(profile),
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->GetDeviceAffiliationIds());
}

}  // namespace

namespace safe_browsing {

// static
ChromeEnterpriseRealTimeUrlLookupServiceFactory*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetInstance() {
  static base::NoDestructor<ChromeEnterpriseRealTimeUrlLookupServiceFactory>
      instance;
  return instance.get();
}

// static
ChromeEnterpriseRealTimeUrlLookupService*
ChromeEnterpriseRealTimeUrlLookupServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<ChromeEnterpriseRealTimeUrlLookupService>(
          profile, /*create=*/true);
}

ChromeEnterpriseRealTimeUrlLookupServiceFactory::
    ChromeEnterpriseRealTimeUrlLookupServiceFactory()
    : ProfileKeyedServiceFactoryIOS(
          "ChromeEnterpriseRealTimeUrlLookupServiceFactory") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(VerdictCacheManagerFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  // SyncServiceFactory dependency through GetUserPopulationForProfile.
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(policy::ManagementServiceIOSFactory::GetInstance());
}

ChromeEnterpriseRealTimeUrlLookupServiceFactory::
    ~ChromeEnterpriseRealTimeUrlLookupServiceFactory() = default;

std::unique_ptr<KeyedService>
ChromeEnterpriseRealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  SafeBrowsingService* safe_browsing_service =
      GetApplicationContext()->GetSafeBrowsingService();
  if (!safe_browsing_service) {
    return nullptr;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  auto* management_service =
      policy::ManagementServiceIOSFactory::GetForProfile(profile);
  CHECK(management_service);

  return std::make_unique<ChromeEnterpriseRealTimeUrlLookupService>(
      safe_browsing_service->GetURLLoaderFactory(),
      VerdictCacheManagerFactory::GetForProfile(profile),
      base::BindRepeating(&GetUserPopulationForProfile, profile),
      std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          identity_manager),
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(profile),
      // Referrer chain provider is not currently supported in iOS.
      /*referrer_chain_provider=*/nullptr, profile->GetPrefs(),
      // TODO(crbug.com/40704516): Pass non-null delegate to display relevant
      // events once chrome://safe-browsing is supported on iOS.
      /*webui_delegate=*/nullptr,
      IdentityManagerFactory::GetForProfile(profile), management_service,
      profile->IsOffTheRecord(),
      /*is_guest_session=*/false,
      base::BindRepeating(&enterprise_connectors::GetProfileEmail,
                          identity_manager),
      base::BindRepeating(&enterprise_connectors::GetActiveContentAreaUser,
                          IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(&IsProfileAffiliated, profile),
      IsCommandLineSwitchEnabled());
}

}  // namespace safe_browsing
