// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/enterprise_proxy_service.h"
#import "components/enterprise/net/core/features.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/proxy/proxy_config.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/base/auth.h"
#import "net/base/proxy_server.h"
#import "net/http/http_response_headers.h"
#import "net/log/net_log_with_source.h"
#import "net/proxy_resolution/proxy_config.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;

using Decision =
    enterprise_net::EnterpriseProxyService::ProxyAuthChallengeMatch::Decision;
using CredentialFetchOutcome =
    enterprise_net::EnterpriseProxyService::CredentialFetchOutcome;

NSString* const kDestinationURL = @"https://destination.example.com/page";
NSString* const kRealm = @"Enterprise Realm";
constexpr std::string_view kProxyHost = "proxy.example.com";
constexpr uint16_t kProxyPort = 8443;
constexpr uint16_t kDefaultHTTPSPort = 443;
constexpr std::string_view kFirstDestinationPattern = "corp1.example.com";
constexpr std::string_view kSecondDestinationPattern = "corp2.example.com";
constexpr char16_t kUsername[] = u"test-user";
constexpr char16_t kPassword[] = u"test-password";

class TestProxyConfigurationProvider : public web::ProxyConfigurationProvider {
 public:
  explicit TestProxyConfigurationProvider(web::BrowserState* browser_state)
      : web::ProxyConfigurationProvider(browser_state) {}

  void UpdateProxyConfiguration(std::vector<web::ProxyRule> rules) override {
    rules_ = std::move(rules);
  }

  const std::vector<web::ProxyRule>& rules() const { return rules_; }

 private:
  std::vector<web::ProxyRule> rules_;
};

net::ProxyConfig::DynamicRoutingConfig CreateSampleRoutingConfig(
    std::string_view destination_pattern) {
  net::ProxyConfig::DynamicRoutingConfig config;
  net::ProxyConfig::DynamicRoutingRule rule;
  rule.proxy_list.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, std::string(kProxyHost), kProxyPort));
  rule.destination_matchers.AddRuleFromString(std::string(destination_pattern));
  config.routing_rules.push_back(std::move(rule));
  return config;
}

// Test double capturing the arguments forwarded by `ProxyServiceController`
// and replying with a configurable outcome. `EnterpriseProxyService` exposes a
// protected default constructor for this purpose, which skips all policy and
// network observer registration.
class FakeEnterpriseProxyService
    : public enterprise_net::EnterpriseProxyService {
 public:
  FakeEnterpriseProxyService() = default;
  ~FakeEnterpriseProxyService() override = default;

  // Sets the decision that classification reports. Only `kNeedsCredentials`
  // reaches the credential fetch; the other decisions are terminal.
  void SetDecision(Decision decision) { decision_ = decision; }

  // Sets what the credential fetch replies with.
  void SetFetchResult(
      CredentialFetchOutcome outcome,
      std::optional<net::AuthCredentials> credentials = std::nullopt) {
    outcome_ = outcome;
    credentials_ = std::move(credentials);
  }

  // Makes the fetch store its reply instead of running it inline, which is the
  // production shape: the reply only comes back after an access token fetch.
  void DeferReply() { defer_reply_ = true; }

  // Runs the deferred reply.
  void RunDeferredReply() {
    std::move(deferred_reply_)
        .Run(outcome_, credentials_, net::NetLogWithSource());
  }

  const std::optional<net::AuthChallengeInfo>& last_auth_info() const {
    return last_auth_info_;
  }
  const GURL& last_destination_url() const { return last_destination_url_; }
  const scoped_refptr<net::HttpResponseHeaders>& last_response_headers() const {
    return last_response_headers_;
  }

  // enterprise_net::EnterpriseProxyService:
  ProxyAuthChallengeMatch ClassifyProxyAuthChallenge(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers)
      override {
    last_auth_info_ = auth_info;
    last_destination_url_ = destination_url;
    last_response_headers_ = response_headers;

    ProxyAuthChallengeMatch match;
    match.decision = decision_;
    return match;
  }

  void FetchProxyAuthCredentials(ProxyAuthChallengeMatch match,
                                 ProxyAuthChallengeCallback callback) override {
    if (defer_reply_) {
      deferred_reply_ = std::move(callback);
      return;
    }
    std::move(callback).Run(outcome_, credentials_, net::NetLogWithSource());
  }

 private:
  Decision decision_ = Decision::kNotApplicable;
  CredentialFetchOutcome outcome_ = CredentialFetchOutcome::kFailure;
  std::optional<net::AuthCredentials> credentials_;
  bool defer_reply_ = false;
  ProxyAuthChallengeCallback deferred_reply_;
  std::optional<net::AuthChallengeInfo> last_auth_info_;
  GURL last_destination_url_;
  scoped_refptr<net::HttpResponseHeaders> last_response_headers_;
};

std::unique_ptr<KeyedService> BuildFakeEnterpriseProxyService(ProfileIOS*) {
  return std::make_unique<FakeEnterpriseProxyService>();
}

// Builds a proxy protection space, defaulting to the HTTPS proxy shape that
// enterprise Provisioning Domain gateways produce. No realm is passed because
// Foundation discards the realm of a proxy protection space; the realm reaches
// the service through the `Proxy-Authenticate` header of the 407 response.
NSURLProtectionSpace* MakeProxyProtectionSpace(
    NSInteger port = kProxyPort,
    NSString* proxy_type = NSURLProtectionSpaceHTTPSProxy) {
  return [[NSURLProtectionSpace alloc]
         initWithProxyHost:base::SysUTF8ToNSString(std::string(kProxyHost))
                      port:port
                      type:proxy_type
                     realm:nil
      authenticationMethod:NSURLAuthenticationMethodHTTPBasic];
}

NSHTTPURLResponse* Make407Response() {
  return [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:kDestinationURL]
        statusCode:407
       HTTPVersion:@"HTTP/1.1"
      headerFields:@{
        @"Proxy-Authenticate" :
            [NSString stringWithFormat:@"Basic realm=\"%@\"", kRealm]
      }];
}

class ProxyServiceControllerTest : public PlatformTest {
 public:
  ProxyServiceControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_net::kEnableDynamicRouteFetching);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(EnterpriseProxyServiceFactoryIOS::GetInstance(),
                              base::BindOnce(&BuildFakeEnterpriseProxyService));
    builder.AddTestingFactory(
        EnterpriseProxyErrorServiceFactoryIOS::GetInstance(),
        EnterpriseProxyErrorServiceFactoryIOS::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    proxy_service_ = static_cast<FakeEnterpriseProxyService*>(
        EnterpriseProxyServiceFactoryIOS::GetForProfile(profile_));
    web_state_.SetBrowserState(profile_);

    proxy_configuration_provider_ =
        std::make_unique<TestProxyConfigurationProvider>(profile_);
    controller_ = std::make_unique<ProxyServiceController>(
        proxy_service_,
        EnterpriseProxyErrorServiceFactoryIOS::GetForProfile(profile_),
        proxy_configuration_provider_.get());
  }

 protected:
  // Runs a challenge through the controller and returns whether it was handled
  // by enterprise proxy logic. Values passed to the callback, if any, are
  // stored in `credentials_future_`.
  bool HandleChallenge(NSURLProtectionSpace* protection_space,
                       NSURLResponse* failure_response) {
    return controller_->MaybeHandleProxyAuthChallenge(
        &web_state_, protection_space, /*proposed_credential=*/nil,
        failure_response, credentials_future_.GetCallback());
  }

  bool HandleDefaultChallenge() {
    return HandleChallenge(MakeProxyProtectionSpace(), Make407Response());
  }

  // Declared first so that the feature state is set before any other member
  // inspects it.
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_ = nullptr;
  raw_ptr<FakeEnterpriseProxyService> proxy_service_ = nullptr;
  web::FakeWebState web_state_;
  std::unique_ptr<TestProxyConfigurationProvider> proxy_configuration_provider_;
  std::unique_ptr<ProxyServiceController> controller_;
  base::test::TestFuture<NSString*, NSString*, NSError*> credentials_future_;
};

// Test that initial routing rules are applied to the proxy provider when the
// controller is created.
TEST_F(ProxyServiceControllerTest, AppliesInitialRoutesOnCreation) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  EXPECT_CALL(mock_service, GetDynamicRoutingConfig())
      .WillOnce(Return(CreateSampleRoutingConfig(kFirstDestinationPattern)));

  TestProxyConfigurationProvider provider(profile_);
  enterprise_net::EnterpriseProxyErrorService error_service(&mock_service);
  ProxyServiceController controller(&mock_service, &error_service, &provider);

  ASSERT_EQ(provider.rules().size(), 1u);
  EXPECT_EQ(provider.rules()[0].match_domains,
            std::vector<std::string>{std::string(kFirstDestinationPattern)});
}

// Test that route update notifications propagate from the enterprise service
// and update the rules in the proxy provider.
TEST_F(ProxyServiceControllerTest, DynamicRoutingUpdatesPropagate) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  EXPECT_CALL(mock_service, GetDynamicRoutingConfig())
      .WillOnce(Return(CreateSampleRoutingConfig(kFirstDestinationPattern)))
      .WillOnce(Return(CreateSampleRoutingConfig(kSecondDestinationPattern)));

  TestProxyConfigurationProvider provider(profile_);
  enterprise_net::EnterpriseProxyErrorService error_service(&mock_service);
  ProxyServiceController controller(&mock_service, &error_service, &provider);

  mock_service.NotifyObservers();

  ASSERT_EQ(provider.rules().size(), 1u);
  EXPECT_EQ(provider.rules()[0].match_domains,
            std::vector<std::string>{std::string(kSecondDestinationPattern)});
}

// Test that when renewed routes are empty, the proxy provider configuration is
// cleared.
TEST_F(ProxyServiceControllerTest, ClearsRoutesWhenUpdatedWithEmptyConfig) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  EXPECT_CALL(mock_service, GetDynamicRoutingConfig())
      .WillOnce(Return(CreateSampleRoutingConfig(kFirstDestinationPattern)))
      .WillOnce(Return(net::ProxyConfig::DynamicRoutingConfig{}));

  TestProxyConfigurationProvider provider(profile_);
  enterprise_net::EnterpriseProxyErrorService error_service(&mock_service);
  ProxyServiceController controller(&mock_service, &error_service, &provider);

  ASSERT_EQ(provider.rules().size(), 1u);

  mock_service.NotifyObservers();

  EXPECT_TRUE(provider.rules().empty());
}

// Test that ProxyServiceControllerFactory creates a controller for a regular
// profile and returns null for an off-the-record profile.
TEST_F(ProxyServiceControllerTest, FactoryCreatesControllerForProfile) {
  TestProfileIOS::Builder builder;
  builder.SetName("factory-profile");
  builder.AddTestingFactory(
      EnterpriseProxyServiceFactoryIOS::GetInstance(),
      base::BindRepeating(
          [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                NiceMock<enterprise_net::MockEnterpriseProxyService>>();
          }));
  builder.AddTestingFactory(
      EnterpriseProxyErrorServiceFactoryIOS::GetInstance(),
      EnterpriseProxyErrorServiceFactoryIOS::GetDefaultFactory());
  builder.AddTestingFactory(ProxyServiceControllerFactory::GetInstance(),
                            ProxyServiceControllerFactory::GetDefaultFactory());
  TestProfileIOS* profile =
      profile_manager_.AddProfileWithBuilder(std::move(builder));

  EXPECT_NE(ProxyServiceControllerFactory::GetForProfile(profile), nullptr);

  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories();
  EXPECT_EQ(ProxyServiceControllerFactory::GetForProfile(otr_profile), nullptr);
}

// Test that a nil protection space is rejected.
TEST_F(ProxyServiceControllerTest, RejectNilProtectionSpace) {
  EXPECT_FALSE(HandleChallenge(/*protection_space=*/nil, Make407Response()));
  EXPECT_FALSE(credentials_future_.IsReady());
  EXPECT_FALSE(proxy_service_->last_auth_info().has_value());
}

// Test that a proxy type without a standard URL scheme is rejected before
// reaching the enterprise proxy service.
TEST_F(ProxyServiceControllerTest, RejectSOCKSProxyChallenge) {
  EXPECT_FALSE(HandleChallenge(
      MakeProxyProtectionSpace(kProxyPort, NSURLProtectionSpaceSOCKSProxy),
      Make407Response()));
  EXPECT_FALSE(credentials_future_.IsReady());
  EXPECT_FALSE(proxy_service_->last_auth_info().has_value());
}

// Test that a challenge for a proxy with no matching routing rule is not
// handled, so that the caller falls back to the default authentication flow.
// The callback must be left unrun: the caller owns a second half of it and
// runs the default flow with it.
TEST_F(ProxyServiceControllerTest, NotApplicableChallengeIsNotHandled) {
  proxy_service_->SetDecision(Decision::kNotApplicable);

  EXPECT_FALSE(HandleDefaultChallenge());
  EXPECT_FALSE(credentials_future_.IsReady());
}

// Test that the decisions which resolve the challenge without ever fetching
// credentials all cancel authentication.
TEST_F(ProxyServiceControllerTest, TerminalDecisionsCancelAuthentication) {
  const struct {
    Decision decision;
    std::string_view name;
  } kCases[] = {
      {Decision::kNoCredentialsNeeded, "kNoCredentialsNeeded"},
      {Decision::kDisguisedError, "kDisguisedError"},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    proxy_service_->SetDecision(test_case.decision);

    EXPECT_TRUE(HandleDefaultChallenge());
    ASSERT_TRUE(credentials_future_.IsReady());
    EXPECT_NSEQ(nil, credentials_future_.Get<0>());
    EXPECT_NSEQ(nil, credentials_future_.Get<1>());
    EXPECT_NSEQ(nil, credentials_future_.Get<2>());
    credentials_future_.Clear();
  }
}

// Test that every unsuccessful credential fetch cancels authentication.
TEST_F(ProxyServiceControllerTest, FailedCredentialFetchCancelsAuthentication) {
  const struct {
    CredentialFetchOutcome outcome;
    std::string_view name;
  } kCases[] = {
      {CredentialFetchOutcome::kFailure, "kFailure"},
      {CredentialFetchOutcome::kSignInRequired, "kSignInRequired"},
  };

  proxy_service_->SetDecision(Decision::kNeedsCredentials);

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    proxy_service_->SetFetchResult(test_case.outcome);

    EXPECT_TRUE(HandleDefaultChallenge());
    ASSERT_TRUE(credentials_future_.IsReady());
    EXPECT_NSEQ(nil, credentials_future_.Get<0>());
    EXPECT_NSEQ(nil, credentials_future_.Get<1>());
    EXPECT_NSEQ(nil, credentials_future_.Get<2>());
    credentials_future_.Clear();
  }
}

// Test that fetched credentials are forwarded back to WebKit as strings.
TEST_F(ProxyServiceControllerTest, FetchedCredentialsAreForwarded) {
  proxy_service_->SetDecision(Decision::kNeedsCredentials);
  proxy_service_->SetFetchResult(CredentialFetchOutcome::kSuccess,
                                 net::AuthCredentials(kUsername, kPassword));

  EXPECT_TRUE(HandleDefaultChallenge());
  ASSERT_TRUE(credentials_future_.IsReady());
  EXPECT_NSEQ(@"test-user", credentials_future_.Get<0>());
  EXPECT_NSEQ(@"test-password", credentials_future_.Get<1>());
  EXPECT_NSEQ(nil, credentials_future_.Get<2>());
}

// Test that a challenge claimed but answered later, which is the production
// shape, still forwards the credentials once the reply arrives.
TEST_F(ProxyServiceControllerTest, DeferredReplyForwardsCredentials) {
  proxy_service_->SetDecision(Decision::kNeedsCredentials);
  proxy_service_->SetFetchResult(CredentialFetchOutcome::kSuccess,
                                 net::AuthCredentials(kUsername, kPassword));
  proxy_service_->DeferReply();

  ASSERT_TRUE(HandleDefaultChallenge());
  ASSERT_FALSE(credentials_future_.IsReady());

  proxy_service_->RunDeferredReply();

  ASSERT_TRUE(credentials_future_.IsReady());
  EXPECT_NSEQ(@"test-user", credentials_future_.Get<0>());
  EXPECT_NSEQ(@"test-password", credentials_future_.Get<1>());
}

// Test that the challenge is translated into the `net::` types expected by the
// enterprise proxy service.
TEST_F(ProxyServiceControllerTest, ChallengeIsConvertedForTheService) {
  proxy_service_->SetDecision(Decision::kDisguisedError);

  ASSERT_TRUE(HandleDefaultChallenge());

  const std::optional<net::AuthChallengeInfo>& auth_info =
      proxy_service_->last_auth_info();
  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->is_proxy);
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ(std::string(kProxyHost), auth_info->challenger.host());
  EXPECT_EQ(kProxyPort, auth_info->challenger.port());
  EXPECT_EQ(base::SysNSStringToUTF8(kRealm), auth_info->realm);
  EXPECT_EQ(GURL(base::SysNSStringToUTF8(kDestinationURL)),
            proxy_service_->last_destination_url());
  ASSERT_TRUE(proxy_service_->last_response_headers());
  EXPECT_EQ(407, proxy_service_->last_response_headers()->response_code());
}

// Test that a challenge reporting no port reaches the service with the default
// port for the proxy scheme, which is required for routing rules to match.
TEST_F(ProxyServiceControllerTest, ChallengeWithoutPortUsesDefaultPort) {
  proxy_service_->SetDecision(Decision::kDisguisedError);

  ASSERT_TRUE(
      HandleChallenge(MakeProxyProtectionSpace(/*port=*/0), Make407Response()));

  ASSERT_TRUE(proxy_service_->last_auth_info().has_value());
  EXPECT_EQ(kDefaultHTTPSPort,
            proxy_service_->last_auth_info()->challenger.port());
}

// Test that a challenge without a response still reaches the service, with no
// headers and an empty destination URL.
TEST_F(ProxyServiceControllerTest, ChallengeWithoutFailureResponse) {
  proxy_service_->SetDecision(Decision::kDisguisedError);

  ASSERT_TRUE(HandleChallenge(MakeProxyProtectionSpace(),
                              /*failure_response=*/nil));

  ASSERT_TRUE(proxy_service_->last_auth_info().has_value());
  EXPECT_TRUE(proxy_service_->last_auth_info()->challenge.empty());
  EXPECT_FALSE(proxy_service_->last_response_headers());
  EXPECT_TRUE(proxy_service_->last_destination_url().is_empty());
}

}  // namespace
