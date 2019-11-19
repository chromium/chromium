// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_factory.h"

#include <memory>

#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/mock_allow_http_auth_preferences.h"
#include "net/http/url_security_manager.h"
#include "net/log/net_log_with_source.h"
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
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));

  // Test what happens with a single scheme.
  registry_factory.RegisterSchemeFactory("Basic", mock_factory_basic);
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            registry_factory.CreateAuthHandlerFromString(
                "Digest", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));

  // Test multiple schemes
  registry_factory.RegisterSchemeFactory("Digest", mock_factory_digest);
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));
  EXPECT_EQ(kDigestReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Digest", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));

  // Test case-insensitivity
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "basic", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));

  // Test replacement of existing auth scheme
  registry_factory.RegisterSchemeFactory("Digest", mock_factory_digest_replace);
  EXPECT_EQ(kBasicReturnCode,
            registry_factory.CreateAuthHandlerFromString(
                "Basic", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));
  EXPECT_EQ(kDigestReturnCodeReplace,
            registry_factory.CreateAuthHandlerFromString(
                "Digest", HttpAuth::AUTH_SERVER, null_ssl_info, gurl,
                NetLogWithSource(), host_resovler.get(), &handler));
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
        server_origin, NetLogWithSource(), host_resolver.get(), &handler);
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
        server_origin, NetLogWithSource(), host_resolver.get(), &handler);
    EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
    EXPECT_TRUE(handler.get() == nullptr);
  }
  {
    std::unique_ptr<HttpAuthHandler> handler;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        "Digest realm=\"FooBar\", nonce=\"xyz\"", HttpAuth::AUTH_PROXY,
        null_ssl_info, proxy_origin, NetLogWithSource(), host_resolver.get(),
        &handler);
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
        "NTLM", HttpAuth::AUTH_SERVER, null_ssl_info, server_origin,
        NetLogWithSource(), host_resolver.get(), &handler);
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
        "Negotiate", HttpAuth::AUTH_SERVER, null_ssl_info, server_origin,
        NetLogWithSource(), host_resolver.get(), &handler);
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

}  // namespace net
