// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include <string>
#include <vector>

#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::CanonicalCookie;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Property;
using ::testing::_;

TEST(OAuthMultiloginResultTest, TryParseCookiesFromValue) {
  OAuthMultiloginResult result("");
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
              ElementsAre(Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true))));
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
      )");
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
      )");
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
      )");
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
  OAuthMultiloginResult result1(data_error_none);
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
  OAuthMultiloginResult result2(data_error_transient);
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
  OAuthMultiloginResult result3(data_error_persistent);
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
  OAuthMultiloginResult result4(data_error_invalid_credentials);
  EXPECT_EQ(result4.status(), OAuthMultiloginResponseStatus::kInvalidTokens);
  EXPECT_THAT(result4.failed_gaia_ids(), ElementsAre(Eq("account1")));

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
      )");
  EXPECT_EQ(unknown_status.status(),
            OAuthMultiloginResponseStatus::kUnknownStatus);
  EXPECT_TRUE(unknown_status.cookies().empty());
}

TEST(OAuthMultiloginResultTest, ParseResponseStatus) {
  struct TestCase {
    std::string status_string;
    OAuthMultiloginResponseStatus expected_status;
  };

  std::vector<TestCase> test_cases = {
      {"FOO", OAuthMultiloginResponseStatus::kUnknownStatus},
      {"OK", OAuthMultiloginResponseStatus::kOk},
      {"RETRY", OAuthMultiloginResponseStatus::kRetry},
      {"INVALID_INPUT", OAuthMultiloginResponseStatus::kInvalidInput},
      {"INVALID_TOKENS", OAuthMultiloginResponseStatus::kInvalidTokens},
      {"ERROR", OAuthMultiloginResponseStatus::kError}};

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.expected_status,
              ParseOAuthMultiloginResponseStatus(test_case.status_string));
  }
}

TEST(OAuthMultiloginResultTest, ParseRealResponseFromGaia_2021_10) {
  OAuthMultiloginResult result("");
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
