// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth.h"

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/http/mock_allow_http_auth_preferences.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

std::unique_ptr<HttpAuthHandlerMock> CreateMockHandler(bool connection_based) {
  std::unique_ptr<HttpAuthHandlerMock> auth_handler =
      std::make_unique<HttpAuthHandlerMock>();
  auth_handler->set_connection_based(connection_based);
  HttpAuthChallengeTokenizer challenge("Basic");
  url::SchemeHostPort scheme_host_port(GURL("https://www.example.com"));
  SSLInfo null_ssl_info;
  EXPECT_TRUE(auth_handler->InitFromChallenge(
      &challenge, HttpAuth::AUTH_SERVER, null_ssl_info,
      NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource()));
  return auth_handler;
}

scoped_refptr<HttpResponseHeaders> HeadersFromResponseText(
    const std::string& response) {
  return base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(response));
}

HttpAuth::AuthorizationResult HandleChallengeResponse(
    bool connection_based,
    const std::string& headers_text,
    std::string* challenge_used) {
  std::unique_ptr<HttpAuthHandlerMock> mock_handler =
      CreateMockHandler(connection_based);
  std::set<HttpAuth::Scheme> disabled_schemes;
  scoped_refptr<HttpResponseHeaders> headers =
      HeadersFromResponseText(headers_text);
  return HttpAuth::HandleChallengeResponse(mock_handler.get(), *headers,
                                           HttpAuth::AUTH_SERVER,
                                           disabled_schemes, challenge_used);
}

}  // namespace

TEST(HttpAuthTest, ChooseBestChallenge) {
  static const struct {
    const char* headers;
    HttpAuth::Scheme challenge_scheme;
    const char* challenge_realm;
  } tests[] = {
      {
          // Basic is the only challenge type, pick it.
          "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate: Basic realm=\"BasicRealm\"\n",

          HttpAuth::AUTH_SCHEME_BASIC,
          "BasicRealm",
      },
      {
          // Fake is the only challenge type, but it is unsupported.
          "Y: Digest realm=\"FooBar\", nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate: Fake realm=\"FooBar\"\n",

          HttpAuth::AUTH_SCHEME_MAX,
          "",
      },
      {
          // Pick Digest over Basic.
          "www-authenticate: Basic realm=\"FooBar\"\n"
          "www-authenticate: Fake realm=\"FooBar\"\n"
          "www-authenticate: nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate: Digest realm=\"DigestRealm\", "
          "nonce=\"aaaaaaaaaa\"\n",

          HttpAuth::AUTH_SCHEME_DIGEST,
          "DigestRealm",
      },
      {
          // Handle an empty header correctly.
          "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate:\n",

          HttpAuth::AUTH_SCHEME_MAX,
          "",
      },
      {
          "WWW-Authenticate: Negotiate\n"
          "WWW-Authenticate: NTLM\n",

#if BUILDFLAG(USE_KERBEROS) && !BUILDFLAG(IS_ANDROID)
          // Choose Negotiate over NTLM on all platforms.
          // TODO(ahendrickson): This may be flaky on Linux and OSX as
          // it relies on being able to load one of the known .so files
          // for gssapi.
          HttpAuth::AUTH_SCHEME_NEGOTIATE,
#else
          // On systems that don't use Kerberos fall back to NTLM.
          HttpAuth::AUTH_SCHEME_NTLM,
#endif  // BUILDFLAG(USE_KERBEROS)
          "",
      },
  };
  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  std::set<HttpAuth::Scheme> disabled_schemes;
  MockAllowHttpAuthPreferences http_auth_preferences;
  auto host_resolver = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  http_auth_handler_factory->SetHttpAuthPreferences(kNegotiateAuthScheme,
                                                    &http_auth_preferences);

  for (const auto& test : tests) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += test.headers;
    scoped_refptr<HttpResponseHeaders> headers =
        HeadersFromResponseText(headers_with_status_line);

    SSLInfo null_ssl_info;
    std::unique_ptr<HttpAuthHandler> handler;
    HttpAuth::ChooseBestChallenge(
        http_auth_handler_factory.get(), *headers, null_ssl_info,
        NetworkAnonymizationKey(), HttpAuth::AUTH_SERVER, scheme_host_port,
        disabled_schemes, NetLogWithSource(), host_resolver.get(), &handler);

    if (handler.get()) {
      EXPECT_EQ(test.challenge_scheme, handler->auth_scheme());
      EXPECT_STREQ(test.challenge_realm, handler->realm().c_str());
    } else {
      EXPECT_EQ(HttpAuth::AUTH_SCHEME_MAX, test.challenge_scheme);
      EXPECT_STREQ("", test.challenge_realm);
    }
  }
}

TEST(HttpAuthTest, HandleChallengeResponse) {
  std::string challenge_used;
  const char* const kMockChallenge =
      "HTTP/1.1 401 Unauthorized\n"
      "WWW-Authenticate: Mock token_here\n";
  const char* const kBasicChallenge =
      "HTTP/1.1 401 Unauthorized\n"
      "WWW-Authenticate: Basic realm=\"happy\"\n";
  const char* const kMissingChallenge =
      "HTTP/1.1 401 Unauthorized\n";
  const char* const kEmptyChallenge =
      "HTTP/1.1 401 Unauthorized\n"
      "WWW-Authenticate: \n";
  const char* const kBasicAndMockChallenges =
      "HTTP/1.1 401 Unauthorized\n"
      "WWW-Authenticate: Basic realm=\"happy\"\n"
      "WWW-Authenticate: Mock token_here\n";
  const char* const kTwoMockChallenges =
      "HTTP/1.1 401 Unauthorized\n"
      "WWW-Authenticate: Mock token_a\n"
      "WWW-Authenticate: Mock token_b\n";

  // Request based schemes should treat any new challenges as rejections of the
  // previous authentication attempt. (There is a slight exception for digest
  // authentication and the stale parameter, but that is covered in the
  // http_auth_handler_digest_unittests).
  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(false, kMockChallenge, &challenge_used));
  EXPECT_EQ("Mock token_here", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(false, kBasicChallenge, &challenge_used));
  EXPECT_EQ("", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(false, kMissingChallenge, &challenge_used));
  EXPECT_EQ("", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(false, kEmptyChallenge, &challenge_used));
  EXPECT_EQ("", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(false, kBasicAndMockChallenges, &challenge_used));
  EXPECT_EQ("Mock token_here", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(false, kTwoMockChallenges, &challenge_used));
  EXPECT_EQ("Mock token_a", challenge_used);

  // Connection based schemes will treat new auth challenges for the same scheme
  // as acceptance (and continuance) of the current approach. If there are
  // no auth challenges for the same scheme, the response will be treated as
  // a rejection.
  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
      HandleChallengeResponse(true, kMockChallenge, &challenge_used));
  EXPECT_EQ("Mock token_here", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(true, kBasicChallenge, &challenge_used));
  EXPECT_EQ("", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(true, kMissingChallenge, &challenge_used));
  EXPECT_EQ("", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_REJECT,
      HandleChallengeResponse(true, kEmptyChallenge, &challenge_used));
  EXPECT_EQ("", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
      HandleChallengeResponse(true, kBasicAndMockChallenges, &challenge_used));
  EXPECT_EQ("Mock token_here", challenge_used);

  EXPECT_EQ(
      HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
      HandleChallengeResponse(true, kTwoMockChallenges, &challenge_used));
  EXPECT_EQ("Mock token_a", challenge_used);
}

TEST(HttpAuthTest, GetChallengeHeaderName) {
  std::string name;

  name = HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_SERVER);
  EXPECT_STREQ("WWW-Authenticate", name.c_str());

  name = HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_PROXY);
  EXPECT_STREQ("Proxy-Authenticate", name.c_str());
}

TEST(HttpAuthTest, GetAuthorizationHeaderName) {
  std::string name;

  name = HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_SERVER);
  EXPECT_STREQ("Authorization", name.c_str());

  name = HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_PROXY);
  EXPECT_STREQ("Proxy-Authorization", name.c_str());
}

}  // namespace net
