// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/enterprise/net/core/enterprise_proxy_service.h"

#import <cstdint>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/net/core/features.h"
#import "components/enterprise/net/core/prefs.h"
#import "components/enterprise/net/core/scoped_extra_allowed_domains_for_testing.h"
#import "components/enterprise/net/core/types.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_network_auth_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "net/base/auth.h"
#import "net/base/url_util.h"
#import "net/log/net_log_with_source.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/scheme_host_port.h"
#import "url/url_constants.h"

namespace {

using enterprise_net::EnterpriseProxyService;
using enterprise_net::ProvisioningDomainProxyConfig;

constexpr char kPvdId[] = "pvd.example.com";
constexpr char kPvdUrl[] = "https://pvd.example.com/.well-known/pvd";

// Routes `*.example.com` through `proxy.example.com`, which requires a profile
// bearer token.
constexpr char kPvdConfigJson[] = R"({
  "identifier": "pvd.example.com",
  "proxies": [{
    "protocol": "https-connect",
    "identity": "proxy",
    "proxy": "https://proxy.example.com:443",
    "google_chrome": {
      "auth": {
        "type": "profile_bearer_token",
        "scope": "cloud_secure_gateway"
      }
    }
  }],
  "proxy-match": [{
    "proxies": ["proxy"],
    "domains": ["*.example.com"]
  }]
})";

constexpr char kProxyHost[] = "proxy.example.com";
constexpr uint16_t kProxyPort = 443;
constexpr char kDestinationUrl[] = "https://app.example.com/";

// Domain that proxy credentials may be sent to in tests.
constexpr char kAllowedDomain[] = "example.com";

// Proxy credentials are only issued for managed accounts, and @google.com
// accounts are known to be managed without fetching account info.
constexpr char kManagedAccountEmail[] = "user@google.com";

constexpr char kUserAgentCredentialKey[] = "x-user-agent";

// Base URL used to read the Basic auth username as a URL query.
constexpr char kUsernameQueryBaseUrl[] = "https://username.example.com/?";

// Returns a `ProxyProvisioningDomains` policy value configuring `kPvdId`.
base::ListValue CreateProxyProvisioningDomainsPolicy() {
  base::ListValue policy;
  policy.Append(base::DictValue()
                    .Set("pvd_id", kPvdId)
                    .Set("auth_config", base::DictValue().Set("type", "none")));
  return policy;
}

}  // namespace

// Tests the `EnterpriseProxyService` built for iOS profiles.
class EnterpriseProxyServiceIOSTest : public PlatformTest {
 protected:
  EnterpriseProxyServiceIOSTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        EnterpriseNetworkAuthServiceFactoryIOS::GetInstance(),
        EnterpriseNetworkAuthServiceFactoryIOS::GetDefaultFactory());
    builder.AddTestingFactory(
        EnterpriseProxyServiceFactoryIOS::GetInstance(),
        EnterpriseProxyServiceFactoryIOS::GetDefaultFactory());
    builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    signin::MakePrimaryAccountAvailable(identity_manager, kManagedAccountEmail,
                                        signin::ConsentLevel::kSignin);
    signin::SetAutomaticIssueOfAccessTokens(identity_manager, true);
  }

  // Initialized first so that the feature is enabled before any thread starts.
  base::test::ScopedFeatureList scoped_feature_list_{
      enterprise_net::kEnableDynamicRouteFetching};
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  enterprise_net::ScopedExtraAllowedDomainsForTesting scoped_allowed_domains_{
      {kAllowedDomain}};
  // Backs the device ID that `ProfileIdService` reads when the PvD fetch
  // resolves its extra headers.
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_ = nullptr;
};

// Tests that proxy credentials carry the web client's mobile user agent, since
// WebKit sends no User-Agent header with proxy CONNECT requests.
TEST_F(EnterpriseProxyServiceIOSTest, CredentialsEmbedMobileUserAgent) {
  EnterpriseProxyService* service =
      EnterpriseProxyServiceFactoryIOS::GetForProfile(profile_);
  ASSERT_TRUE(service);

  test_url_loader_factory_.AddResponse(kPvdUrl, kPvdConfigJson);
  profile_->GetPrefs()->SetList(enterprise_net::kProxyProvisioningDomains,
                                CreateProxyProvisioningDomainsPolicy());
  ASSERT_TRUE(base::test::RunUntil([service] {
    std::vector<ProvisioningDomainProxyConfig> configs =
        service->GetProvisioningDomainConfigs();
    return !configs.empty() &&
           configs[0].state == ProvisioningDomainProxyConfig::State::kValid;
  }));

  net::AuthChallengeInfo auth_info;
  auth_info.is_proxy = true;
  auth_info.challenger =
      url::SchemeHostPort(url::kHttpsScheme, kProxyHost, kProxyPort);
  EnterpriseProxyService::ProxyAuthChallengeMatch match =
      service->ClassifyProxyAuthChallenge(auth_info, GURL(kDestinationUrl),
                                          /*response_headers=*/nullptr);
  ASSERT_EQ(EnterpriseProxyService::ProxyAuthChallengeMatch::Decision::
                kNeedsCredentials,
            match.decision);

  base::test::TestFuture<EnterpriseProxyService::CredentialFetchOutcome,
                         const std::optional<net::AuthCredentials>&,
                         const net::NetLogWithSource&>
      future;
  service->FetchProxyAuthCredentials(std::move(match), future.GetCallback());
  ASSERT_EQ(EnterpriseProxyService::CredentialFetchOutcome::kSuccess,
            future.Get<0>());
  ASSERT_TRUE(future.Get<1>().has_value());

  std::string user_agent;
  ASSERT_TRUE(net::GetValueForKeyInQuery(
      GURL(base::StrCat({kUsernameQueryBaseUrl,
                         base::UTF16ToUTF8(future.Get<1>()->username())})),
      kUserAgentCredentialKey, &user_agent));
  EXPECT_EQ(web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE),
            user_agent);
}
