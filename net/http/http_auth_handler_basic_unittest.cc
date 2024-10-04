// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_basic.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_info.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

TEST(HttpAuthHandlerBasicTest, GenerateAuthToken) {
  static const struct {
    const char* username;
    const char* password;
    const char* expected_credentials;
  } tests[] = {
    { "foo", "bar", "Basic Zm9vOmJhcg==" },
    // Empty username
    { "", "foobar", "Basic OmZvb2Jhcg==" },
    // Empty password
    { "anon", "", "Basic YW5vbjo=" },
    // Empty username and empty password.
    { "", "", "Basic Og==" },
  };
  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  HttpAuthHandlerBasic::Factory factory;
  for (const auto& test : tests) {
    std::string challenge = "Basic realm=\"Atlantis\"";
    SSLInfo null_ssl_info;
    auto host_resolver = std::make_unique<MockHostResolver>();
    std::unique_ptr<HttpAuthHandler> basic;
    EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
                      challenge, HttpAuth::AUTH_SERVER, null_ssl_info,
                      NetworkAnonymizationKey(), scheme_host_port,
                      NetLogWithSource(), host_resolver.get(), &basic));
    AuthCredentials credentials(base::ASCIIToUTF16(test.username),
                                base::ASCIIToUTF16(test.password));
    HttpRequestInfo request_info;
    std::string auth_token;
    TestCompletionCallback callback;
    int rv = basic->GenerateAuthToken(&credentials, &request_info,
                                      callback.callback(), &auth_token);
    EXPECT_THAT(rv, IsOk());
    EXPECT_STREQ(test.expected_credentials, auth_token.c_str());
  }
}

TEST(HttpAuthHandlerBasicTest, HandleAnotherChallenge) {
  static const struct {
    const char* challenge;
    HttpAuth::AuthorizationResult expected_rv;
  } tests[] = {
    // The handler is initialized using this challenge.  The first
    // time HandleAnotherChallenge is called with it should cause it
    // to treat the second challenge as a rejection since it is for
    // the same realm.
    {
      "Basic realm=\"First\"",
      HttpAuth::AUTHORIZATION_RESULT_REJECT
    },

    // A challenge for a different realm.
    {
      "Basic realm=\"Second\"",
      HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM
    },

    // Although RFC 2617 isn't explicit about this case, if there is
    // more than one realm directive, we pick the last one.  So this
    // challenge should be treated as being for "First" realm.
    {
      "Basic realm=\"Second\",realm=\"First\"",
      HttpAuth::AUTHORIZATION_RESULT_REJECT
    },

    // And this one should be treated as if it was for "Second."
    {
      "basic realm=\"First\",realm=\"Second\"",
      HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM
    }
  };

  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  HttpAuthHandlerBasic::Factory factory;
  SSLInfo null_ssl_info;
  auto host_resolver = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandler> basic;
  EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
                    tests[0].challenge, HttpAuth::AUTH_SERVER, null_ssl_info,
                    NetworkAnonymizationKey(), scheme_host_port,
                    NetLogWithSource(), host_resolver.get(), &basic));

  for (const auto& test : tests) {
    HttpAuthChallengeTokenizer tok(test.challenge);
    EXPECT_EQ(test.expected_rv, basic->HandleAnotherChallenge(&tok));
  }
}

TEST(HttpAuthHandlerBasicTest, InitFromChallenge) {
  static const struct {
    const char* challenge;
    int expected_rv;
    const char* expected_realm;
  } tests[] = {
    // No realm (we allow this even though realm is supposed to be required
    // according to RFC 2617.)
    {
      "Basic",
      OK,
      "",
    },

    // Realm is empty string.
    {
      "Basic realm=\"\"",
      OK,
      "",
    },

    // Realm is valid.
    {
      "Basic realm=\"test_realm\"",
      OK,
      "test_realm",
    },

    // The parser ignores tokens which aren't known.
    {
      "Basic realm=\"test_realm\",unknown_token=foobar",
      OK,
      "test_realm",
    },

    // The parser skips over tokens which aren't known.
    {
      "Basic unknown_token=foobar,realm=\"test_realm\"",
      OK,
      "test_realm",
    },

#if 0
    // TODO(cbentzel): It's unclear what the parser should do in these cases.
    //                 It seems like this should either be treated as invalid,
    //                 or the spaces should be used as a separator.
    {
      "Basic realm=\"test_realm\" unknown_token=foobar",
      OK,
      "test_realm",
    },

    // The parser skips over tokens which aren't known.
    {
      "Basic unknown_token=foobar realm=\"test_realm\"",
      OK,
      "test_realm",
    },
#endif

    // The parser fails when the first token is not "Basic".
    {
      "Negotiate",
      ERR_INVALID_RESPONSE,
      ""
    },

    // Although RFC 2617 isn't explicit about this case, if there is
    // more than one realm directive, we pick the last one.
    {
      "Basic realm=\"foo\",realm=\"bar\"",
      OK,
      "bar",
    },

    // Handle ISO-8859-1 character as part of the realm. The realm is converted
    // to UTF-8.
    {
      "Basic realm=\"foo-\xE5\"",
      OK,
      "foo-\xC3\xA5",
    },
  };
  HttpAuthHandlerBasic::Factory factory;
  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  for (const auto& test : tests) {
    std::string challenge = test.challenge;
    SSLInfo null_ssl_info;
    auto host_resolver = std::make_unique<MockHostResolver>();
    std::unique_ptr<HttpAuthHandler> basic;
    int rv = factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_SERVER, null_ssl_info,
        NetworkAnonymizationKey(), scheme_host_port, NetLogWithSource(),
        host_resolver.get(), &basic);
    EXPECT_EQ(test.expected_rv, rv);
    if (rv == OK)
      EXPECT_EQ(test.expected_realm, basic->realm());
  }
}

// Test that when Basic is configured to forbid HTTP, attempting to create a
// Basic auth handler for a HTTP context is rejected.
TEST(HttpAuthHandlerBasicTest, BasicAuthRequiresHTTPS) {
  url::SchemeHostPort nonsecure_scheme_host_port(
      GURL("http://www.example.com"));
  HttpAuthHandlerBasic::Factory factory;
  HttpAuthPreferences http_auth_preferences;
  http_auth_preferences.set_basic_over_http_enabled(false);
  factory.set_http_auth_preferences(&http_auth_preferences);

  std::string challenge = "Basic realm=\"Atlantis\"";
  SSLInfo null_ssl_info;
  auto host_resolver = std::make_unique<MockHostResolver>();
  std::unique_ptr<HttpAuthHandler> basic;

  // Ensure that HTTP is disallowed.
  EXPECT_THAT(factory.CreateAuthHandlerFromString(
                  challenge, HttpAuth::AUTH_SERVER, null_ssl_info,
                  NetworkAnonymizationKey(), nonsecure_scheme_host_port,
                  NetLogWithSource(), host_resolver.get(), &basic),
              IsError(ERR_UNSUPPORTED_AUTH_SCHEME));

  // Ensure that HTTPS is allowed.
  url::SchemeHostPort secure_scheme_host_port(GURL("https://www.example.com"));
  EXPECT_THAT(factory.CreateAuthHandlerFromString(
                  challenge, HttpAuth::AUTH_SERVER, null_ssl_info,
                  NetworkAnonymizationKey(), secure_scheme_host_port,
                  NetLogWithSource(), host_resolver.get(), &basic),
              IsOk());
}

}  // namespace net
