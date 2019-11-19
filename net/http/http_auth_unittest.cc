// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth.h"

#include <memory>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
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

namespace net {

namespace {

std::unique_ptr<HttpAuthHandlerMock> CreateMockHandler(bool connection_based) {
  std::unique_ptr<HttpAuthHandlerMock> auth_handler =
      std::make_unique<HttpAuthHandlerMock>();
  auth_handler->set_connection_based(connection_based);
  std::string challenge_text = "Basic";
  HttpAuthChallengeTokenizer challenge(challenge_text.begin(),
                                         challenge_text.end());
  GURL origin("www.example.com");
  SSLInfo null_ssl_info;
  EXPECT_TRUE(auth_handler->InitFromChallenge(&challenge, HttpAuth::AUTH_SERVER,
                                              null_ssl_info, origin,
                                              NetLogWithSource()));
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

          HttpAuth::AUTH_SCHEME_BASIC, "BasicRealm",
      },
      {
          // Fake is the only challenge type, but it is unsupported.
          "Y: Digest realm=\"FooBar\", nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate: Fake realm=\"FooBar\"\n",

          HttpAuth::AUTH_SCHEME_MAX, "",
      },
      {
          // Pick Digest over Basic.
          "www-authenticate: Basic realm=\"FooBar\"\n"
          "www-authenticate: Fake realm=\"FooBar\"\n"
          "www-authenticate: nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate: Digest realm=\"DigestRealm\", "
          "nonce=\"aaaaaaaaaa\"\n",

          HttpAuth::AUTH_SCHEME_DIGEST, "DigestRealm",
      },
      {
          // Handle an empty header correctly.
          "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
          "www-authenticate:\n",

          HttpAuth::AUTH_SCHEME_MAX, "",
      },
      {
          "WWW-Authenticate: Negotiate\n"
          "WWW-Authenticate: NTLM\n",

#if BUILDFLAG(USE_KERBEROS) && !defined(OS_ANDROID)
          // Choose Negotiate over NTLM on all platforms.
          // TODO(ahendrickson): This may be flaky on Linux and OSX as it
          // relies on being able to load one of the known .so files
          // for gssapi.
          HttpAuth::AUTH_SCHEME_NEGOTIATE,
#else
          // On systems that don't use Kerberos fall back to NTLM.
          HttpAuth::AUTH_SCHEME_NTLM,
#endif  // BUILDFLAG(USE_KERBEROS)
          "",
      }};
  GURL origin("http://www.example.com");
  std::set<HttpAuth::Scheme> disabled_schemes;
  MockAllowHttpAuthPreferences http_auth_preferences;
  std::unique_ptr<HostResolver> host_resolver(new MockHostResolver());
  std::unique_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  http_auth_handler_factory->SetHttpAuthPreferences(kNegotiateAuthScheme,
                                                    &http_auth_preferences);

  for (size_t i = 0; i < base::size(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<HttpResponseHeaders> headers =
        HeadersFromResponseText(headers_with_status_line);

    SSLInfo null_ssl_info;
    std::unique_ptr<HttpAuthHandler> handler;
    HttpAuth::ChooseBestChallenge(http_auth_handler_factory.get(), *headers,
                                  null_ssl_info, HttpAuth::AUTH_SERVER, origin,
                                  disabled_schemes, NetLogWithSource(),
                                  host_resolver.get(), &handler);

    if (handler.get()) {
      EXPECT_EQ(tests[i].challenge_scheme, handler->auth_scheme());
      EXPECT_STREQ(tests[i].challenge_realm, handler->realm().c_str());
    } else {
      EXPECT_EQ(HttpAuth::AUTH_SCHEME_MAX, tests[i].challenge_scheme);
      EXPECT_STREQ("", tests[i].challenge_realm);
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
