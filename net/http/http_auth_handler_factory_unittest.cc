// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_factory.h"

#include <memory>

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

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

class MockHttpAuthHandlerFactory : public HttpAuthHandlerFactory {
 public:
  explicit MockHttpAuthHandlerFactory(int return_code) :
      return_code_(return_code) {}
  ~MockHttpAuthHandlerFactory() override = default;

  int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                        HttpAuth::Target target,
                        const SSLInfo& ssl_info,
                        const NetworkIsolationKey& network_isolation_key,
                        const GURL& origin,
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
  HttpAuthHandlerRegistryFactory registry_factory;
  GURL gurl("www.google.com");
  const int kBasicReturnCode = -1;
  MockHttpAuthHandlerFactory* mock_factory_basic =
      new MockHttpAuthHandlerFactory(kBasicReturnCode);

  const int kDigestReturnCode = -2;
  MockHttpAuthHandlerFactory* mock_factory_digest =
      new MockHttpAuthHandlerFactory(kDigestReturnCode);

  const int kDigestReturnCodeReplace = -3;
  MockHttpAuthHandlerFactory* mock_factory_digest_replace =
      new MockHttpAuthHandlerFactory(kDigestReturnCodeReplace);

  auto host_resovler = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandler> handler;

  // No schemes should be supported in the beginning.
  EXPECT_EQ(
      ERR_UNSUPPORTED_AUTH_SCHEME,
      registry_factory.CreateAuthHandlerFromString(
          "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));

  // Test what happens with a single scheme.
  registry_factory.RegisterSchemeFactory("Basic", mock_factory_basic);
  EXPECT_EQ(
      kBasicReturnCode,
      registry_factory.CreateAuthHandlerFromString(
          "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));
  EXPECT_EQ(
      ERR_UNSUPPORTED_AUTH_SCHEME,
      registry_factory.CreateAuthHandlerFromString(
          "Digest", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));

  // Test multiple schemes
  registry_factory.RegisterSchemeFactory("Digest", mock_factory_digest);
  EXPECT_EQ(
      kBasicReturnCode,
      registry_factory.CreateAuthHandlerFromString(
          "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));
  EXPECT_EQ(
      kDigestReturnCode,
      registry_factory.CreateAuthHandlerFromString(
          "Digest", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));

  // Test case-insensitivity
  EXPECT_EQ(
      kBasicReturnCode,
      registry_factory.CreateAuthHandlerFromString(
          "basic", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));

  // Test replacement of existing auth scheme
  registry_factory.RegisterSchemeFactory("Digest", mock_factory_digest_replace);
  EXPECT_EQ(
      kBasicReturnCode,
      registry_factory.CreateAuthHandlerFromString(
          "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));
  EXPECT_EQ(
      kDigestReturnCodeReplace,
      registry_factory.CreateAuthHandlerFromString(
          "Digest", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
          gurl, NetLogWithSource(), host_resovler.get(), &handler));
}

TEST(HttpAuthHandlerFactoryTest, DefaultFactory) {
  std::unique_ptr<HostResolver> host_resolver(new MockHostResolver());
  MockAllowHttpAuthPreferences http_auth_preferences;
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  http_auth_handler_factory->SetHttpAuthPreferences(kNegotiateAuthScheme,
                                                    &http_auth_preferences);
  GURL server_origin("http://www.example.com");
  GURL proxy_origin("http://cache.example.com:3128");
  SSLInfo null_ssl_info;
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "Basic realm=\"FooBar\"", HttpAuth::AUTH_SERVER, null_ssl_info,
        NetworkIsolationKey(), server_origin, NetLogWithSource(),
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
        NetworkIsolationKey(), server_origin, NetLogWithSource(),
        host_resolver.get(), &handler);
    EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
    EXPECT_TRUE(handler.get() == nullptr);
  }
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "Digest realm=\"FooBar\", nonce=\"xyz\"", HttpAuth::AUTH_PROXY,
        null_ssl_info, NetworkIsolationKey(), proxy_origin, NetLogWithSource(),
        host_resolver.get(), &handler);
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
        "NTLM", HttpAuth::AUTH_SERVER, null_ssl_info, NetworkIsolationKey(),
        server_origin, NetLogWithSource(), host_resolver.get(), &handler);
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
        NetworkIsolationKey(), server_origin, NetLogWithSource(),
        host_resolver.get(), &handler);
// Note the default factory doesn't support Kerberos on Android
#if BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)
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
#endif  // BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)
  }
}

TEST(HttpAuthHandlerFactoryTest, BasicFactoryRespectsHTTPEnabledPref) {
  std::unique_ptr<HostResolver> host_resolver(new MockHostResolver());
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
    // Challenges that result in success results.
    {OK, secure_origin, "Basic realm=\"FooBar\""},
    {OK, secure_origin, "Digest realm=\"FooBar\", nonce=\"xyz\""},
    {OK, nonsecure_origin, "Digest realm=\"FooBar\", nonce=\"xyz\""},
    {OK, secure_origin, "Ntlm"},
    {OK, nonsecure_origin, "Ntlm"},
#if BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)
    {OK, secure_origin, "Negotiate"},
    {OK, nonsecure_origin, "Negotiate"},
#endif
    // Challenges that result in error results.
    {ERR_UNSUPPORTED_AUTH_SCHEME, nonsecure_origin, "Basic realm=\"FooBar\""},
  };

  for (const auto target : kTargets) {
    for (const TestCase& test_case : kTestCases) {
      std::unique_ptr<HttpAuthHandler> handler;
      int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
          test_case.challenge, target, null_ssl_info, NetworkIsolationKey(),
          test_case.origin, NetLogWithSource(), host_resolver.get(), &handler);
      EXPECT_THAT(rv, IsError(test_case.expected_net_error));
    }
  }
}

TEST(HttpAuthHandlerFactoryTest, LogCreateAuthHandlerResults) {
  std::unique_ptr<HostResolver> host_resolver(new MockHostResolver());
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  GURL origin("http://www.example.com");
  SSLInfo null_ssl_info;
  RecordingBoundTestNetLog test_net_log;

  net::NetLogCaptureMode capture_modes[] = {
      NetLogCaptureMode::kDefault, NetLogCaptureMode::kIncludeSensitive};

  struct TestCase {
    int expected_net_error;
    const char* challenge;
    const net::HttpAuth::Target auth_target;
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
    test_net_log.SetObserverCaptureMode(capture_mode);

    // ... evaluate the expected results for each test case.
    for (auto test_case : test_cases) {
      std::unique_ptr<HttpAuthHandler> handler;
      int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
          test_case.challenge, test_case.auth_target, null_ssl_info,
          NetworkIsolationKey(), origin, test_net_log.bound(),
          host_resolver.get(), &handler);
      EXPECT_THAT(rv, IsError(test_case.expected_net_error));
      auto entries = test_net_log.GetEntriesWithType(
          NetLogEventType::AUTH_HANDLER_CREATE_RESULT);
      ASSERT_EQ(1u, entries.size());
      const std::string* scheme = entries[0].params.FindStringKey("scheme");
      ASSERT_NE(nullptr, scheme);
      EXPECT_STRCASEEQ(test_case.expected_scheme, scheme->data());
      base::Optional<int> net_error = entries[0].params.FindIntKey("net_error");
      if (test_case.expected_net_error) {
        ASSERT_TRUE(net_error.has_value());
        EXPECT_EQ(test_case.expected_net_error, net_error.value());
      } else {
        ASSERT_FALSE(net_error.has_value());
      }

      // The challenge should be logged only when sensitive logging is enabled.
      const std::string* challenge =
          entries[0].params.FindStringKey("challenge");
      if (capture_mode == NetLogCaptureMode::kDefault) {
        ASSERT_EQ(nullptr, challenge);
      } else {
        ASSERT_NE(nullptr, challenge);
        EXPECT_EQ(net::NetLogStringValue(test_case.challenge).GetString(),
                  challenge->data());
      }

      test_net_log.Clear();
    }
  }
}

}  // namespace net
