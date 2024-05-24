// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/mock_allow_http_auth_preferences.h"
#include "net/http/url_security_manager.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

class MockHttpAuthHandlerFactory : public HttpAuthHandlerFactory {
 public:
  explicit MockHttpAuthHandlerFactory(int return_code) :
      return_code_(return_code) {}
  ~MockHttpAuthHandlerFactory() override = default;

  int CreateAuthHandler(
      HttpAuthChallengeTokenizer* challenge,
      HttpAuth::Target target,
      const SSLInfo& ssl_info,
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::SchemeHostPort& scheme_host_port,
      CreateReason reason,
      int nonce_count,
      const NetLogWithSource& net_log,
      HostResolver* host_resolver,
      std::unique_ptr<HttpAuthHandler>* handler) override {
    handler->reset();
    return return_code_;
  }

 private:
  int return_code_;
};

}  // namespace

TEST(HttpAuthHandlerFactoryTest, RegistryFactory) {
  SSLInfo null_ssl_info;
  HttpAuthHandlerRegistryFactory registry_factory(
      /*http_auth_preferences=*/nullptr);
  url::SchemeHostPort scheme_host_port(GURL("https://www.google.com"));
  const int kBasicReturnCode = -1;
  auto mock_factory_basic =
      std::make_unique<MockHttpAuthHandlerFactory>(kBasicReturnCode);

  const int kDigestReturnCode = -2;
  auto mock_factory_digest =
      std::make_unique<MockHttpAuthHandlerFactory>(kDigestReturnCode);

  const int kDigestReturnCodeReplace = -3;
  auto mock_factory_digest_replace =
      std::make_unique<MockHttpAuthHandlerFactory>(kDigestReturnCodeReplace);

  auto host_resovler = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandler> handler;

  // No schemes should be supported in the beginning.
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));

  // Test what happens with a single scheme.
  registry_factory.RegisterSchemeFactory("Basic",
                                         std::move(mock_factory_basic));
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            registry_factory.CreateAuthHandlerFromString(
                "Digest", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));

  // Test multiple schemes
  registry_factory.RegisterSchemeFactory("Digest",
                                         std::move(mock_factory_digest));
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));
  EXPECT_EQ(kDigestReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Digest", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));

  // Test case-insensitivity
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));

  // Test replacement of existing auth scheme
  registry_factory.RegisterSchemeFactory(
      "Digest", std::move(mock_factory_digest_replace));
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));
  EXPECT_EQ(kDigestReturnCodeReplace,
            registry_factory.CreateAuthHandlerFromString(
                "Digest", HttpAuth::AUTH_SERVER, null_ssl_info,
                NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
                host_resovler.get(), &handler));
}

TEST(HttpAuthHandlerFactoryTest, DefaultFactory) {
  auto host_resolver = std::make_unique<MockHostResolver>();
  MockAllowHttpAuthPreferences http_auth_preferences;
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  http_auth_handler_factory->SetHttpAuthPreferences(kNegotiateAuthScheme,
                                                    &http_auth_preferences);
  url::SchemeHostPort server_scheme_host_port(GURL("http://www.example.com"));
  url::SchemeHostPort proxy_scheme_host_port(
      GURL("http://cache.example.com:3128"));
  SSLInfo null_ssl_info;
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "Basic realm=\"FooBar\"", HttpAuth::AUTH_SERVER, null_ssl_info,
        NetworkAnonymizationKey(), server_scheme_host_port, NetLogWithSource(),
        host_resolver.get(), &handler);
    EXPECT_THAT(rv, IsOk());
    ASSERT_FALSE(handler.get() == nullptr);
    EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, handler->auth_scheme());
    EXPECT_STREQ("FooBar", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_SERVER, handler->target());
    EXPECT_FALSE(handler->encrypts_identity());
    EXPECT_FALSE(handler->is_connection_based());
  }
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "UNSUPPORTED realm=\"FooBar\"", HttpAuth::AUTH_SERVER, null_ssl_info,
        NetworkAnonymizationKey(), server_scheme_host_port, NetLogWithSource(),
        host_resolver.get(), &handler);
    EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
    EXPECT_TRUE(handler.get() == nullptr);
  }
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "Digest realm=\"FooBar\", nonce=\"xyz\"", HttpAuth::AUTH_PROXY,
        null_ssl_info, NetworkAnonymizationKey(), proxy_scheme_host_port,
        NetLogWithSource(), host_resolver.get(), &handler);
    EXPECT_THAT(rv, IsOk());
    ASSERT_FALSE(handler.get() == nullptr);
    EXPECT_EQ(HttpAuth::AUTH_SCHEME_DIGEST, handler->auth_scheme());
    EXPECT_STREQ("FooBar", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_PROXY, handler->target());
    EXPECT_TRUE(handler->encrypts_identity());
    EXPECT_FALSE(handler->is_connection_based());
  }
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "NTLM", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkAnonymizationKey(),
        server_scheme_host_port, NetLogWithSource(), host_resolver.get(),
        &handler);
    EXPECT_THAT(rv, IsOk());
    ASSERT_FALSE(handler.get() == nullptr);
    EXPECT_EQ(HttpAuth::AUTH_SCHEME_NTLM, handler->auth_scheme());
    EXPECT_STREQ("", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_SERVER, handler->target());
    EXPECT_TRUE(handler->encrypts_identity());
    EXPECT_TRUE(handler->is_connection_based());
  }
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "Negotiate", HttpAuth::AUTH_SERVER, null_ssl_info,
        NetworkAnonymizationKey(), server_scheme_host_port, NetLogWithSource(),
        host_resolver.get(), &handler);
// Note the default factory doesn't support Kerberos on Android
#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
    EXPECT_THAT(rv, IsOk());
    ASSERT_FALSE(handler.get() == nullptr);
    EXPECT_EQ(HttpAuth::AUTH_SCHEME_NEGOTIATE, handler->auth_scheme());
    EXPECT_STREQ("", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_SERVER, handler->target());
    EXPECT_TRUE(handler->encrypts_identity());
    EXPECT_TRUE(handler->is_connection_based());
#else
    EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
    EXPECT_TRUE(handler.get() == nullptr);
#endif  // BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
  }
}

TEST(HttpAuthHandlerFactoryTest, HttpAuthUrlFilter) {
  auto host_resolver = std::make_unique<MockHostResolver>();

  MockAllowHttpAuthPreferences http_auth_preferences;
  // Set the Preference that blocks Basic Auth over HTTP on all of the
  // factories. It shouldn't impact any behavior except for the Basic factory.
  http_auth_preferences.set_basic_over_http_enabled(false);
  // Set the preference that only allows "https://www.example.com" to use HTTP
  // auth.
  http_auth_preferences.set_http_auth_scheme_filter(
      base::BindRepeating([](const url::SchemeHostPort& scheme_host_port) {
        return scheme_host_port ==
               url::SchemeHostPort(GURL("https://www.example.com"));
      }));

  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault(&http_auth_preferences));

  GURL nonsecure_origin("http://www.example.com");
  GURL secure_origin("https://www.example.com");

  SSLInfo null_ssl_info;
  const HttpAuth::Target kTargets[] = {HttpAuth::AUTH_SERVER,
                                       HttpAuth::AUTH_PROXY};
  struct TestCase {
    int expected_net_error;
    const GURL origin;
    const char* challenge;
  } const kTestCases[] = {
    {OK, secure_origin, "Basic realm=\"FooBar\""},
    {ERR_UNSUPPORTED_AUTH_SCHEME, nonsecure_origin, "Basic realm=\"FooBar\""},
    {OK, secure_origin, "Digest realm=\"FooBar\", nonce=\"xyz\""},
    {OK, nonsecure_origin, "Digest realm=\"FooBar\", nonce=\"xyz\""},
    {OK, secure_origin, "Ntlm"},
    {OK, nonsecure_origin, "Ntlm"},
#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
    {OK, secure_origin, "Negotiate"},
    {OK, nonsecure_origin, "Negotiate"},
#endif
  };

  for (const auto target : kTargets) {
    for (const TestCase& test_case : kTestCases) {
      std::unique_ptr<HttpAuthHandler> handler;
      int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
          test_case.challenge, target, null_ssl_info, NetworkAnonymizationKey(),
          url::SchemeHostPort(test_case.origin), NetLogWithSource(),
          host_resolver.get(), &handler);
      EXPECT_THAT(rv, IsError(test_case.expected_net_error));
    }
  }
}

TEST(HttpAuthHandlerFactoryTest, BasicFactoryRespectsHTTPEnabledPref) {
  auto host_resolver = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());

  // Set the Preference that blocks Basic Auth over HTTP on all of the
  // factories. It shouldn't impact any behavior except for the Basic factory.
  MockAllowHttpAuthPreferences http_auth_preferences;
  http_auth_preferences.set_basic_over_http_enabled(false);
  http_auth_handler_factory->SetHttpAuthPreferences(kBasicAuthScheme,
                                                    &http_auth_preferences);
  http_auth_handler_factory->SetHttpAuthPreferences(kDigestAuthScheme,
                                                    &http_auth_preferences);
  http_auth_handler_factory->SetHttpAuthPreferences(kNtlmAuthScheme,
                                                    &http_auth_preferences);
  http_auth_handler_factory->SetHttpAuthPreferences(kNegotiateAuthScheme,
                                                    &http_auth_preferences);

  url::SchemeHostPort nonsecure_scheme_host_port(
      GURL("http://www.example.com"));
  url::SchemeHostPort secure_scheme_host_port(GURL("https://www.example.com"));
  SSLInfo null_ssl_info;

  const HttpAuth::Target kTargets[] = {HttpAuth::AUTH_SERVER,
                                       HttpAuth::AUTH_PROXY};
  struct TestCase {
    int expected_net_error;
    const url::SchemeHostPort scheme_host_port;
    const char* challenge;
  } const kTestCases[] = {
    // Challenges that result in success results.
    {OK, secure_scheme_host_port, "Basic realm=\"FooBar\""},
    {OK, secure_scheme_host_port, "Digest realm=\"FooBar\", nonce=\"xyz\""},
    {OK, nonsecure_scheme_host_port, "Digest realm=\"FooBar\", nonce=\"xyz\""},
    {OK, secure_scheme_host_port, "Ntlm"},
    {OK, nonsecure_scheme_host_port, "Ntlm"},
#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
    {OK, secure_scheme_host_port, "Negotiate"},
    {OK, nonsecure_scheme_host_port, "Negotiate"},
#endif
    // Challenges that result in error results.
    {ERR_UNSUPPORTED_AUTH_SCHEME, nonsecure_scheme_host_port,
     "Basic realm=\"FooBar\""},
  };

  for (const auto target : kTargets) {
    for (const TestCase& test_case : kTestCases) {
      std::unique_ptr<HttpAuthHandler> handler;
      int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
          test_case.challenge, target, null_ssl_info, NetworkAnonymizationKey(),
          test_case.scheme_host_port, NetLogWithSource(), host_resolver.get(),
          &handler);
      EXPECT_THAT(rv, IsError(test_case.expected_net_error));
    }
  }
}

TEST(HttpAuthHandlerFactoryTest, LogCreateAuthHandlerResults) {
  auto host_resolver = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  SSLInfo null_ssl_info;
  RecordingNetLogObserver net_log_observer;

  NetLogCaptureMode capture_modes[] = {NetLogCaptureMode::kDefault,
                                       NetLogCaptureMode::kIncludeSensitive};

  struct TestCase {
    int expected_net_error;
    const char* challenge;
    const HttpAuth::Target auth_target;
    const char* expected_scheme;
  } test_cases[] = {
      // Challenges that result in success results.
      {OK, "Basic realm=\"FooBar\"", HttpAuth::AUTH_SERVER, "Basic"},
      {OK, "Basic realm=\"FooBar\"", HttpAuth::AUTH_PROXY, "Basic"},
      {OK, "Digest realm=\"FooBar\", nonce=\"xyz\"", HttpAuth::AUTH_SERVER,
       "Digest"},
      // Challenges that result in error results.
      {ERR_INVALID_RESPONSE, "", HttpAuth::AUTH_SERVER, ""},
      {ERR_INVALID_RESPONSE, "Digest realm=\"no_nonce\"", HttpAuth::AUTH_SERVER,
       "Digest"},
      {ERR_UNSUPPORTED_AUTH_SCHEME, "UNSUPPORTED realm=\"FooBar\"",
       HttpAuth::AUTH_SERVER, "UNSUPPORTED"},
      {ERR_UNSUPPORTED_AUTH_SCHEME, "invalid\xff\x0a", HttpAuth::AUTH_SERVER,
       "%ESCAPED:\xE2\x80\x8B invalid%FF\n"},
      {ERR_UNSUPPORTED_AUTH_SCHEME, "UNSUPPORTED2 realm=\"FooBar\"",
       HttpAuth::AUTH_PROXY, "UNSUPPORTED2"}};

  // For each level of capture sensitivity...
  for (auto capture_mode : capture_modes) {
    net_log_observer.SetObserverCaptureMode(capture_mode);

    // ... evaluate the expected results for each test case.
    for (auto test_case : test_cases) {
      std::unique_ptr<HttpAuthHandler> handler;
      int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
          test_case.challenge, test_case.auth_target, null_ssl_info,
          NetworkAnonymizationKey(), scheme_host_port,
          NetLogWithSource::Make(NetLogSourceType::NONE), host_resolver.get(),
          &handler);
      EXPECT_THAT(rv, IsError(test_case.expected_net_error));
      auto entries = net_log_observer.GetEntriesWithType(
          NetLogEventType::AUTH_HANDLER_CREATE_RESULT);
      ASSERT_EQ(1u, entries.size());
      const std::string* scheme = entries[0].params.FindString("scheme");
      ASSERT_NE(nullptr, scheme);
      EXPECT_STRCASEEQ(test_case.expected_scheme, scheme->data());
      std::optional<int> net_error = entries[0].params.FindInt("net_error");
      if (test_case.expected_net_error) {
        ASSERT_TRUE(net_error.has_value());
        EXPECT_EQ(test_case.expected_net_error, net_error.value());
      } else {
        ASSERT_FALSE(net_error.has_value());
      }

      // The challenge should be logged only when sensitive logging is enabled.
      const std::string* challenge = entries[0].params.FindString("challenge");
      if (capture_mode == NetLogCaptureMode::kDefault) {
        ASSERT_EQ(nullptr, challenge);
      } else {
        ASSERT_NE(nullptr, challenge);
        EXPECT_EQ(NetLogStringValue(test_case.challenge).GetString(),
                  challenge->data());
      }

      net_log_observer.Clear();
    }
  }
}

}  // namespace net
