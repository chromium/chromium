// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/token_binding_response_encryption_error.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::CanonicalCookie;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using Credential = ::RegisterBoundSessionPayload::Credential;
using DeviceBoundSession = ::OAuthMultiloginResult::DeviceBoundSession;
using Scope = ::RegisterBoundSessionPayload::Scope;
using SessionScope = ::RegisterBoundSessionPayload::SessionScope;

using enum ::OAuthMultiloginResult::DeviceBoundSession::Domain;

namespace {

constexpr char kInvalidTokensResponseFormat[] =
    R"()]}'
        {
          "status": "%s",
          "failed_accounts": [
            {
              "status": "RECOVERABLE",
              "obfuscated_id": "account1",
              "token_binding_retry_response": {
                "challenge": "test_challenge1"
              }
            },
            {
              "status": "OK",
              "obfuscated_id": "account2"
            },
            {
              "status": "RECOVERABLE",
              "obfuscated_id": "account3",
              "token_binding_retry_response": {
                "challenge": "test_challenge3"
              }
            },
            {
              "status": "NON_RECOVERABLE",
              "obfuscated_id": "account4"
            }
          ]
        }
      )";

constexpr char kResponseWithEncryptedCookiesFormat[] =
    R"()]}'
        {
          "status": "OK",
          "token_binding_directed_response": {},
          "cookies":[
            {
              "name":"SID",
              "value":"%s",
              "domain":".google.com",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            },
            {
              "name":"SAPISID",
              "value":"%s",
              "host":"google.com",
              "path":"/",
              "isSecure":false,
              "isHttpOnly":true,
              "priority":"HIGH",
              "maxAge":63070000,
              "sameSite":"Lax"
            },
            {
              "name":"APISID",
              "value":"%s",
              "host":"google.com",
              "path":"/",
              "isSecure":false,
              "isHttpOnly":true,
              "priority":"HIGH",
              "maxAge":63070000,
              "sameSite":"Lax"
            }
          ]
        }
      )";

static constexpr char kEncryptionErrorHistogram[] =
    "Signin.OAuthMultiloginResponseEncryptionError";

}  // namespace

TEST(OAuthMultiloginResultTest, TryParseCookiesFromValue) {
  OAuthMultiloginResult result(OAuthMultiloginResponseStatus::kOk);
  // SID: typical response for a domain cookie
  // SAPISID: typical response for a host cookie
  // SSID: not canonical cookie because of the wrong path, should not be added
  // HSID: canonical but not valid because of the wrong host value, still will
  // be parsed but domain_ field will be empty. Also it is expired.
  std::string data =
      R"({
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            },
            {
              "name":"SAPISID",
              "value":"vAlUe2",
              "host":"google.com",
              "path":"/",
              "isSecure":false,
              "isHttpOnly":true,
              "priority":"HIGH",
              "maxAge":63070000,
              "sameSite":"Lax"
            },
            {
              "name":"SSID",
              "value":"vAlUe3",
              "domain":".google.de",
              "path":"path",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000,
            },
            {
              "name":"HSID",
              "value":"vAlUe4",
              "host":".google.fr",
              "path":"/",
              "priority":"HIGH",
              "maxAge":0,
              "sameSite":"Strict"
            },
            {
              "name": "__Secure-1PSID",
              "value": "vAlUe4",
              "domain": ".google.fr",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 63072000,
              "priority": "HIGH",
            }
          ]
        }
      )";

  result.TryParseCookiesFromValue(base::test::ParseJsonDict(data));

  base::Time time_now = base::Time::Now();
  base::Time expiration_time = (time_now + base::Seconds(34560000.));
  double now = time_now.InSecondsFSinceUnixEpoch();
  double expiration = expiration_time.InSecondsFSinceUnixEpoch();
  const std::vector<CanonicalCookie> cookies = {
      *CanonicalCookie::CreateUnsafeCookieForTesting(
          "SID", "vAlUe1", ".google.ru", "/", time_now, time_now,
          expiration_time, time_now, /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
          net::CookiePriority::COOKIE_PRIORITY_HIGH),
      *CanonicalCookie::CreateUnsafeCookieForTesting(
          "SAPISID", "vAlUe2", "google.com", "/", time_now, time_now,
          expiration_time, time_now, /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::LAX_MODE,
          net::CookiePriority::COOKIE_PRIORITY_HIGH),
      *CanonicalCookie::CreateUnsafeCookieForTesting(
          "HSID", "vAlUe4", "", "/", time_now, time_now, time_now, time_now,
          /*secure=*/true, /*httponly=*/true, net::CookieSameSite::STRICT_MODE,
          net::CookiePriority::COOKIE_PRIORITY_HIGH),
      *CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Secure-1PSID", "vAlUe4", ".google.fr", "/", time_now, time_now,
          expiration_time, time_now, /*secure=*/true, /*httponly=*/true,
          net::CookieSameSite::UNSPECIFIED,
          net::CookiePriority::COOKIE_PRIORITY_HIGH)};

  EXPECT_EQ((int)result.cookies().size(), 4);

  EXPECT_TRUE(result.cookies()[0].IsEquivalent(cookies[0]));
  EXPECT_TRUE(result.cookies()[1].IsEquivalent(cookies[1]));
  EXPECT_TRUE(result.cookies()[2].IsEquivalent(cookies[2]));
  EXPECT_TRUE(result.cookies()[3].IsEquivalent(cookies[3]));

  EXPECT_FALSE(result.cookies()[0].IsExpired(base::Time::Now()));
  EXPECT_FALSE(result.cookies()[1].IsExpired(base::Time::Now()));
  EXPECT_TRUE(result.cookies()[2].IsExpired(base::Time::Now()));
  EXPECT_FALSE(result.cookies()[3].IsExpired(base::Time::Now()));

  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::IsDomainCookie, Eq(true)),
                  Property(&CanonicalCookie::IsHostCookie, Eq(true)),
                  Property(&CanonicalCookie::IsDomainCookie, Eq(false)),
                  Property(&CanonicalCookie::IsDomainCookie, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsCanonical, IsTrue()),
                          Property(&CanonicalCookie::IsCanonical, IsTrue()),
                          Property(&CanonicalCookie::IsCanonical, IsTrue()),
                          Property(&CanonicalCookie::IsCanonical, IsTrue())));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsHttpOnly, Eq(false)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(true)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(true)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(true))));
  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::SecureAttribute, Eq(true)),
                  Property(&CanonicalCookie::SecureAttribute, Eq(false)),
                  Property(&CanonicalCookie::SecureAttribute, Eq(true)),
                  Property(&CanonicalCookie::SecureAttribute, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::UNSPECIFIED)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::LAX_MODE)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::STRICT_MODE)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::UNSPECIFIED))));
  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH))));

  EXPECT_THAT(result.cookies()[0].CreationDate().InSecondsFSinceUnixEpoch(),
              DoubleNear(now, 0.5));
  EXPECT_THAT(result.cookies()[0].LastAccessDate().InSecondsFSinceUnixEpoch(),
              DoubleNear(now, 0.5));
  EXPECT_THAT(result.cookies()[0].ExpiryDate().InSecondsFSinceUnixEpoch(),
              DoubleNear(expiration, 0.5));
  EXPECT_THAT(result.cookies()[0].LastUpdateDate().InSecondsFSinceUnixEpoch(),
              DoubleNear(now, 0.5));
}

TEST(OAuthMultiloginResultTest, CreateOAuthMultiloginResultFromString) {
  OAuthMultiloginResult result1(R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )",
                                net::HTTP_OK);
  EXPECT_EQ(result1.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_FALSE(result1.cookies().empty());

  OAuthMultiloginResult result2(R"(many_random_characters_before_newline
        {
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )",
                                net::HTTP_OK);
  EXPECT_EQ(result2.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_FALSE(result2.cookies().empty());

  OAuthMultiloginResult result3(R"())]}\'\n)]}'\n{
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )",
                                net::HTTP_OK);
  EXPECT_EQ(result3.status(), OAuthMultiloginResponseStatus::kUnknownStatus);
}

TEST(OAuthMultiloginResultTest, ProduceErrorFromResponseStatus) {
  std::string data_error_none =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )";
  OAuthMultiloginResult result1(data_error_none, net::HTTP_OK);
  EXPECT_EQ(result1.status(), OAuthMultiloginResponseStatus::kOk);

  std::string data_error_transient =
      R"(()]}'
        {
          "status": "RETRY",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )";
  OAuthMultiloginResult result2(data_error_transient,
                                net::HTTP_SERVICE_UNAVAILABLE);
  EXPECT_EQ(result2.status(), OAuthMultiloginResponseStatus::kRetry);

  // "ERROR" is a real response status that Gaia sends. This is a persistent
  // error.
  std::string data_error_persistent =
      R"(()]}'
        {
          "status": "ERROR",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )";
  OAuthMultiloginResult result3(data_error_persistent,
                                net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_EQ(result3.status(), OAuthMultiloginResponseStatus::kError);

  std::string data_error_invalid_credentials =
      R"()]}'
        {
          "status": "INVALID_TOKENS",
          "failed_accounts": [
            {
              "status": "RECOVERABLE",
              "obfuscated_id": "account1"
            },
            {
              "status": "OK",
              "obfuscated_id": "account2"
            }
          ],
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )";
  OAuthMultiloginResult result4(data_error_invalid_credentials,
                                net::HTTP_FORBIDDEN);
  EXPECT_EQ(result4.status(), OAuthMultiloginResponseStatus::kInvalidTokens);
  EXPECT_THAT(result4.failed_accounts(),
              ElementsAre(FieldsAre(GaiaId("account1"), "")));

  // Unknown status.
  OAuthMultiloginResult unknown_status(R"()]}'
        {
          "status": "Foo",
          "cookies":[
            {
              "name":"SID",
              "value":"vAlUe1",
              "domain":".google.ru",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            }
          ]
        }
      )",
                                       net::HTTP_OK);
  EXPECT_EQ(unknown_status.status(),
            OAuthMultiloginResponseStatus::kUnknownStatus);
  EXPECT_TRUE(unknown_status.cookies().empty());
}

TEST(OAuthMultiloginResultTest, ParseResponseStatus) {
  struct TestCase {
    std::string status_string;
    int http_response_code;
    OAuthMultiloginResponseStatus expected_status;
  };

  std::vector<TestCase> test_cases = {
      {"FOO", net::HTTP_OK, OAuthMultiloginResponseStatus::kUnknownStatus},
      {"OK", net::HTTP_OK, OAuthMultiloginResponseStatus::kOk},
      {"RETRY", net::HTTP_SERVICE_UNAVAILABLE,
       OAuthMultiloginResponseStatus::kRetry},
      {"RETRY", net::HTTP_BAD_REQUEST,
       OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge},
      {"INVALID_INPUT", net::HTTP_BAD_REQUEST,
       OAuthMultiloginResponseStatus::kInvalidInput},
      {"INVALID_TOKENS", net::HTTP_FORBIDDEN,
       OAuthMultiloginResponseStatus::kInvalidTokens},
      {"ERROR", net::HTTP_INTERNAL_SERVER_ERROR,
       OAuthMultiloginResponseStatus::kError}};

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected_status,
              ParseOAuthMultiloginResponseStatus(test_case.status_string,
                                                 test_case.http_response_code));
  }
}

TEST(OAuthMultiloginResultTest, ParseRealResponseFromGaia_2021_10) {
  OAuthMultiloginResult result(OAuthMultiloginResponseStatus::kOk);
  // SID: typical response for a domain cookie
  // SAPISID: typical response for a host cookie
  // SSID: not canonical cookie because of the wrong path, should not be added
  // HSID: canonical but not valid because of the wrong host value, still will
  // be parsed but domain_ field will be empty. Also it is expired.
  std::string data =
      R"({
  "status": "OK",
  "cookies": [
    {
      "name": "SID",
      "value": "FAKE_SID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Secure-1PSID",
      "value": "FAKE___Secure-1PSID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Secure-3PSID",
      "value": "FAKE___Secure-3PSID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "HSID",
      "value": "FAKE_HSID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "SSID",
      "value": "FAJE_SSID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "APISID",
      "value": "FAKE_APISID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "SAPISID",
      "value": "FAKE_SAPISID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Secure-1PAPISID",
      "value": "FAKE___Secure-1PAPISID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Secure-3PAPISID",
      "value": "FAKE___Secure-3PAPISID_VALUE",
      "domain": ".google.fr",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "SID",
      "value": "FAKE_SID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Secure-1PSID",
      "value": "FAKE___Secure-1PSID",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Secure-3PSID",
      "value": "FAKE___Secure-3PSID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "HSID",
      "value": "FAKE_HSID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "SSID",
      "value": "FAKE_SID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "APISID",
      "value": "FAKE_APISID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "SAPISID",
      "value": "FAKE_APISID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Secure-1PAPISID",
      "value": "FAKE___Secure-1PAPISID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Secure-3PAPISID",
      "value": "FAKE___Secure-3PAPISID_VALUE",
      "domain": ".youtube.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "SID",
      "value": "FAKE_SID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Secure-1PSID",
      "value": "FAKE___Secure-1PSID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Secure-3PSID",
      "value": "FAKE___Secure-3PSID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "LSID",
      "value": "FAKE_LSID_VALUE",
      "host": "accounts.google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Host-1PLSID",
      "value": "FAKE___Host-1PLSID_VALUE",
      "host": "accounts.google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Host-3PLSID",
      "value": "FAKE___Host-3PLSID_VALUE",
      "host": "accounts.google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "HSID",
      "value": "FAKE_HSID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "SSID",
      "value": "FAKE_SAPISID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "APISID",
      "value": "FAKE_APISID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": false,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "SAPISID",
      "value": "FAKE_APISID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH"
    },
    {
      "name": "__Secure-1PAPISID",
      "value": "FAKE___Secure-1PAPISID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH",
    },
    {
      "name": "__Secure-3PAPISID",
      "value": "FAKE___Secure-3PAPISID_VALUE",
      "domain": ".google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": false,
      "maxAge": 63072000,
      "priority": "HIGH",
      "sameSite": "none"
    },
    {
      "name": "__Host-GAPS",
      "value": "FAKE___Host-GAPS_VALUE",
      "host": "accounts.google.com",
      "path": "/",
      "isSecure": true,
      "isHttpOnly": true,
      "maxAge": 63072000,
      "priority": "HIGH"
    }
  ],
  "accounts": [
    {
      "type": "PERSON_ACCOUNT",
      "display_name": "PERSON_1",
      "display_email": "person_1@gmail.com",
      "photo_url": "https://lh3.googleusercontent.com/.../photo.jpg",
      "selected": false,
      "default_user": true,
      "authuser": 0,
      "valid_session": true,
      "obfuscated_id": "1234567890",
      "is_verified": true
    },
    {
      "type": "PERSON_ACCOUNT",
      "display_name": "PERSON_2",
      "display_email": "bling.person_2@gmail.com",
      "photo_url": "https://lh3.googleusercontent.com/.../photo.jpg",
      "selected": false,
      "default_user": false,
      "authuser": 1,
      "valid_session": true,
      "obfuscated_id": "9876543210",
      "is_verified": true
    }
  ]
})";

  result.TryParseCookiesFromValue(base::test::ParseJsonDict(data));

  ASSERT_EQ((int)result.cookies().size(), 31);

  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::Name, Eq("SID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-1PSID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-3PSID")),
                  Property(&CanonicalCookie::Name, Eq("HSID")),
                  Property(&CanonicalCookie::Name, Eq("SSID")),
                  Property(&CanonicalCookie::Name, Eq("APISID")),
                  Property(&CanonicalCookie::Name, Eq("SAPISID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-1PAPISID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-3PAPISID")),

                  Property(&CanonicalCookie::Name, Eq("SID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-1PSID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-3PSID")),
                  Property(&CanonicalCookie::Name, Eq("HSID")),
                  Property(&CanonicalCookie::Name, Eq("SSID")),
                  Property(&CanonicalCookie::Name, Eq("APISID")),
                  Property(&CanonicalCookie::Name, Eq("SAPISID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-1PAPISID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-3PAPISID")),

                  Property(&CanonicalCookie::Name, Eq("SID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-1PSID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-3PSID")),
                  Property(&CanonicalCookie::Name, Eq("LSID")),
                  Property(&CanonicalCookie::Name, Eq("__Host-1PLSID")),
                  Property(&CanonicalCookie::Name, Eq("__Host-3PLSID")),
                  Property(&CanonicalCookie::Name, Eq("HSID")),
                  Property(&CanonicalCookie::Name, Eq("SSID")),
                  Property(&CanonicalCookie::Name, Eq("APISID")),
                  Property(&CanonicalCookie::Name, Eq("SAPISID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-1PAPISID")),
                  Property(&CanonicalCookie::Name, Eq("__Secure-3PAPISID")),

                  Property(&CanonicalCookie::Name, Eq("__Host-GAPS"))));
}

TEST(OAuthMultiloginResultTest, ParseRetryResponseWithTokenBindingChallenge) {
  OAuthMultiloginResult result(
      base::StringPrintf(kInvalidTokensResponseFormat, "RETRY"),
      net::HTTP_BAD_REQUEST);
  EXPECT_EQ(result.status(),
            OAuthMultiloginResponseStatus::kRetryWithTokenBindingChallenge);
  EXPECT_THAT(result.failed_accounts(),
              ElementsAre(FieldsAre(GaiaId("account1"), "test_challenge1"),
                          FieldsAre(GaiaId("account3"), "test_challenge3"),
                          FieldsAre(GaiaId("account4"), std::string())));
}

TEST(OAuthMultiloginResultTest,
     ParseInvalidTokensResponseWithTokenBindingChallenge) {
  // Token binding challenges are ignored if status is INVALID_TOKENS.
  OAuthMultiloginResult result(
      base::StringPrintf(kInvalidTokensResponseFormat, "INVALID_TOKENS"),
      net::HTTP_FORBIDDEN);
  EXPECT_EQ(result.status(), OAuthMultiloginResponseStatus::kInvalidTokens);
  EXPECT_THAT(result.failed_accounts(),
              ElementsAre(FieldsAre(GaiaId("account1"), std::string()),
                          FieldsAre(GaiaId("account3"), std::string()),
                          FieldsAre(GaiaId("account4"), std::string())));
}

// Decryptor is successfully used if cookies in the response are encrypted.
TEST(OAuthMultiloginResultTest, ParseEncryptedCookies) {
  base::HistogramTester histogram_tester;
  std::string response = base::StringPrintf(kResponseWithEncryptedCookiesFormat,
                                            "vAlUe1", "vAlUe2", "vAlUe3");
  auto decryptor = [](std::string_view encrypted_cookie) {
    return base::StrCat({encrypted_cookie, ".decrypted"});
  };

  OAuthMultiloginResult result(response, net::HTTP_OK,
                               base::BindRepeating(decryptor));

  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::Value, "vAlUe1.decrypted"),
                  Property(&CanonicalCookie::Value, "vAlUe2.decrypted"),
                  Property(&CanonicalCookie::Value, "vAlUe3.decrypted")));
  histogram_tester.ExpectUniqueSample(
      kEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kSuccessfullyDecrypted,
      /*expected_bucket_count=*/3);
}

// Decryptor is ignored if cookies in the response are not encrypted.
TEST(OAuthMultiloginResultTest, ParseCookiesIgnoresDecryptor) {
  constexpr char kResponseWithNonEncryptedCookiesFormat[] =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name":"SID",
              "value":"%s",
              "domain":".google.com",
              "path":"/",
              "isSecure":true,
              "isHttpOnly":false,
              "priority":"HIGH",
              "maxAge":63070000
            },
            {
              "name":"SAPISID",
              "value":"%s",
              "host":"google.com",
              "path":"/",
              "isSecure":false,
              "isHttpOnly":true,
              "priority":"HIGH",
              "maxAge":63070000,
              "sameSite":"Lax"
            }
          ]
        }
      )";
  base::HistogramTester histogram_tester;
  std::string response = base::StringPrintf(
      kResponseWithNonEncryptedCookiesFormat, "vAlUe1", "vAlUe2");
  auto decryptor = [](std::string_view encrypted_cookie) {
    ADD_FAILURE()
        << "Decryptor was called unexpectedly for cookie with value \""
        << encrypted_cookie << "\"";
    return std::string();
  };

  OAuthMultiloginResult result(response, net::HTTP_OK,
                               base::BindRepeating(decryptor));

  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::Value, "vAlUe1"),
                          Property(&CanonicalCookie::Value, "vAlUe2")));
  histogram_tester.ExpectUniqueSample(
      kEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kSuccessNoEncryption,
      /*expected_bucket_count=*/2);
}

// Result is set to failure if cookies are encrypted but a decryptor is not set.
TEST(OAuthMultiloginResultTest, ParseEncryptedCookiesFailsWithoutDecryptor) {
  base::HistogramTester histogram_tester;
  std::string response = base::StringPrintf(kResponseWithEncryptedCookiesFormat,
                                            "vAlUe1", "vAlUe2", "vAlUe3");

  OAuthMultiloginResult result(response, net::HTTP_OK, base::NullCallback());

  EXPECT_EQ(result.status(), OAuthMultiloginResponseStatus::kUnknownStatus);
  EXPECT_THAT(result.cookies(), IsEmpty());
  histogram_tester.ExpectUniqueSample(
      kEncryptionErrorHistogram,
      TokenBindingResponseEncryptionError::kResponseUnexpectedlyEncrypted,
      /*expected_bucket_count=*/3);
}

// Cookies that failed to decrypt are omitted from the result.
TEST(OAuthMultiloginResultTest, ParseEncryptedCookiesDecryptionFails) {
  base::HistogramTester histogram_tester;
  std::string response = base::StringPrintf(kResponseWithEncryptedCookiesFormat,
                                            "vAlUe1", "vAlUe2", "vAlUe3");
  auto decryptor = [](std::string_view encrypted_cookie) {
    return encrypted_cookie != "vAlUe2"
               ? std::string()
               : base::StrCat({encrypted_cookie, ".decrypted"});
  };

  OAuthMultiloginResult result(response, net::HTTP_OK,
                               base::BindRepeating(decryptor));

  EXPECT_THAT(result.cookies(), ElementsAre(Property(&CanonicalCookie::Value,
                                                     "vAlUe2.decrypted")));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kEncryptionErrorHistogram),
      ElementsAre(
          base::Bucket(TokenBindingResponseEncryptionError::kDecryptionFailed,
                       /*count=*/2),
          base::Bucket(
              TokenBindingResponseEncryptionError::kSuccessfullyDecrypted,
              /*count=*/1)));
}

TEST(OAuthMultiloginResultTest, NoDeviceBoundSessionInfo) {
  base::HistogramTester histogram_tester;

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {}
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(result.device_bound_sessions(), IsEmpty());

  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      /*expected_count=*/0);
}

TEST(OAuthMultiloginResultTest, ReuseExistingDeviceBoundSession) {
  base::HistogramTester histogram_tester;

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(result.device_bound_sessions(),
              UnorderedElementsAre(
                  AllOf(Field(&DeviceBoundSession::is_device_bound, true),
                        Field(&DeviceBoundSession::domain, kGoogle),
                        Field(&DeviceBoundSession::register_session_payload,
                              Eq(std::nullopt)))));

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::kNone,
      /*expected_bucket_count=*/1);
}

TEST(OAuthMultiloginResultTest, RegisterNewDeviceBoundSession) {
  base::HistogramTester histogram_tester;

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            },
            {
              "name": "__Secure-Google-Cookie",
              "value": "secure-google-cookie-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(
      result.device_bound_sessions(),
      UnorderedElementsAre(
          AllOf(
              Field(&DeviceBoundSession::is_device_bound, true),
              Field(&DeviceBoundSession::domain, kGoogle),
              Field(&DeviceBoundSession::register_session_payload,
                    Optional(AllOf(
                        Field(&RegisterBoundSessionPayload::session_id, "id"),
                        Field(&RegisterBoundSessionPayload::refresh_url,
                              "/RotateBoundCookies"),
                        Field(&RegisterBoundSessionPayload::credentials,
                              UnorderedElementsAre(AllOf(
                                  Field(&Credential::name, "__Secure-1PSIDTS"),
                                  Field(&Credential::scope,
                                        AllOf(Field(&Scope::domain,
                                                    ".google.com"),
                                              Field(&Scope::path, "/")))))))))),
          AllOf(Field(&DeviceBoundSession::is_device_bound, true),
                Field(&DeviceBoundSession::domain, kGoogle))));

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::kNone,
      /*expected_bucket_count=*/1);
}

TEST(OAuthMultiloginResultTest, GetDeviceBoundSessionsToRegister) {
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[],
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": false
            }
          ]
        }
      )";

  const OAuthMultiloginResult result(raw_data, net::HTTP_OK);

  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  // Not bound sessions are filtered out during parsing.
  EXPECT_THAT(result.device_bound_sessions(), SizeIs(2));
  EXPECT_THAT(
      result.GetDeviceBoundSessionsToRegister(),
      UnorderedElementsAre(Pointee(Field(
          &DeviceBoundSession::register_session_payload,
          Optional(Field(&RegisterBoundSessionPayload::session_id, "id"))))));
}

TEST(OAuthMultiloginResultTest, UnknownDeviceBoundSessionDomain) {
  base::HistogramTester histogram_tester;

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "unknown-cookie",
              "value": "unknown-cookie-value",
              "domain": ".unknown.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "UNKNOWN_COM",
              "is_device_bound": true
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(result.device_bound_sessions(),
              UnorderedElementsAre(
                  AllOf(Field(&DeviceBoundSession::is_device_bound, true),
                        Field(&DeviceBoundSession::domain, kGoogle))));

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::kNone,
      /*expected_bucket_count=*/1);
}

TEST(OAuthMultiloginResultTest, IsNotDeviceBoundSession) {
  base::HistogramTester histogram_tester;

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": false
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(result.device_bound_sessions(), IsEmpty());

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::kNone,
      /*expected_bucket_count=*/1);
}

TEST(OAuthMultiloginResultTest, RegisterNewDeviceBoundSessionInvalidPayload) {
  base::HistogramTester histogram_tester;

  // The payload is invalid because it's missing the `session_identifier` field.
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "credentials": [
                  {
                    "type": "cookie",
                    "name": "__Secure-1PSIDTS",
                    "scope": {
                      "domain": ".google.com",
                      "path": "/"
                    }
                  }
                ],
                "refresh_url": "/RotateBoundCookies"
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }));
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(result.device_bound_sessions(), IsEmpty());

  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::
          kRegisterPayloadRequiredFieldMissing,
      /*expected_bucket_count=*/1);
}

TEST(OAuthMultiloginResultTest, RegisterNewStandardDeviceBoundSession) {
  base::HistogramTester histogram_tester;

  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            },
            {
              "name": "__Secure-Google-Cookie",
              "value": "secure-google-cookie-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "refresh_url": "/RotateBoundCookies",
                "scope": {
                  "origin": "https://google.com",
                  "include_site": true,
                  "scope_specification" : [
                    {
                      "type": "include",
                      "domain": ".google.com",
                      "path": "/"
                    }
                  ]
                },
                "credentials": [{
                  "type": "cookie",
                  "name": "__Secure-1PSIDTS",
                  "attributes": "Domain=.google.com; Path=/; Secure"
                }]
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }),
      /*standard_device_bound_session_credentials=*/true);
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(
      result.device_bound_sessions(),
      UnorderedElementsAre(AllOf(
          Field(&DeviceBoundSession::is_device_bound, true),
          Field(&DeviceBoundSession::domain, kGoogle),
          Field(
              &DeviceBoundSession::register_session_payload,
              Optional(AllOf(
                  Field(&RegisterBoundSessionPayload::session_id, "id"),
                  Field(&RegisterBoundSessionPayload::refresh_url,
                        "/RotateBoundCookies"),
                  Field(
                      &RegisterBoundSessionPayload::scope,
                      AllOf(
                          Field(&SessionScope::origin, "https://google.com"),
                          Field(&SessionScope::include_site, true),
                          Field(&SessionScope::specifications,
                                UnorderedElementsAre(AllOf(
                                    Field(&Scope::type, Scope::Type::kInclude),
                                    Field(&Scope::domain, ".google.com"),
                                    Field(&Scope::path, "/")))))),
                  Field(
                      &RegisterBoundSessionPayload::credentials,
                      UnorderedElementsAre(AllOf(
                          Field(&Credential::name, "__Secure-1PSIDTS"),
                          Field(&Credential::type, "cookie"),
                          Field(&Credential::attributes,
                                "Domain=.google.com; Path=/; Secure"))))))))));

  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::kNone,
      /*expected_bucket_count=*/1);
}

TEST(OAuthMultiloginResultTest, GetStandardDeviceBoundSessionsToRegister) {
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[],
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "refresh_url": "/RotateBoundCookies",
                "scope": {
                  "origin": "https://google.com",
                  "include_site": true,
                  "scope_specification" : [
                    {
                      "type": "include",
                      "domain": ".google.com",
                      "path": "/"
                    }
                  ]
                },
                "credentials": [{
                  "type": "cookie",
                  "name": "__Secure-1PSIDTS",
                  "attributes": "Domain=.google.com; Path=/; Secure"
                }]
              }
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            },
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": false
            }
          ]
        }
      )";

  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/base::NullCallback(),
      /*standard_device_bound_session_credentials=*/true);

  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  // Not bound sessions are filtered out during parsing.
  EXPECT_THAT(result.device_bound_sessions(), SizeIs(2));
  EXPECT_THAT(
      result.GetDeviceBoundSessionsToRegister(),
      UnorderedElementsAre(Pointee(Field(
          &DeviceBoundSession::register_session_payload,
          Optional(Field(&RegisterBoundSessionPayload::session_id, "id"))))));
}

TEST(OAuthMultiloginResultTest,
     RegisterNewStandardDeviceBoundSessionInvalidPayload) {
  base::HistogramTester histogram_tester;

  // The payload is invalid because it has the `scope::scope_specification`
  // field malformed.
  const std::string raw_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            },
            {
              "name": "__Secure-Google-Cookie",
              "value": "secure-google-cookie-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": true,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "token_binding_directed_response": {},
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "refresh_url": "/RotateBoundCookies",
                "scope": {
                  "origin": "https://google.com",
                  "include_site": true,
                  "scope_specification" : [ "malformed" ]
                },
                "credentials": [{
                  "type": "cookie",
                  "name": "__Secure-1PSIDTS",
                  "attributes": "Domain=.google.com; Path=/; Secure"
                }]
              }
            }
          ]
        }
      )";
  const OAuthMultiloginResult result(
      raw_data, net::HTTP_OK,
      /*cookie_decryptor=*/
      base::BindLambdaForTesting([](std::string_view encrypted_cookie) {
        return base::StrCat({encrypted_cookie, ".decrypted"});
      }),
      /*standard_device_bound_session_credentials=*/true);
  ASSERT_EQ(result.status(), OAuthMultiloginResponseStatus::kOk);
  EXPECT_THAT(result.device_bound_sessions(), IsEmpty());

  histogram_tester.ExpectTotalCount(
      "Signin.BoundSessionCredentials.OAuthMultilogin.UnknownDomain",
      /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(
      "Signin.BoundSessionCredentials.OAuthMultilogin.ParsingError",
      OAuthMultiloginDeviceBoundSessionParsingError::
          kRegisterPayloadMalformedSessionScopeSpecification,
      /*expected_bucket_count=*/1);
}
