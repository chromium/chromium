// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
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
              "maxAge":63070000
            },
            {
              "name":"HSID",
              "value":"vAlUe4",
              "host":".google.fr",
              "path":"/",
              "priority":"HIGH",
              "maxAge":0,
              "sameSite":"Strict"
            }
          ]
        }
      )";

  std::unique_ptr<base::DictionaryValue> dictionary_value =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(data));
  result.TryParseCookiesFromValue(dictionary_value.get());

  base::Time time_now = base::Time::Now();
  base::Time expiration_time =
      (time_now + base::TimeDelta::FromSecondsD(63070000.));
  double now = time_now.ToDoubleT();
  double expiration = expiration_time.ToDoubleT();
  const std::vector<CanonicalCookie> cookies = {
      CanonicalCookie("SID", "vAlUe1", ".google.ru", "/", time_now, time_now,
                      expiration_time, /*is_secure=*/true,
                      /*is_http_only=*/false, net::CookieSameSite::UNSPECIFIED,
                      net::CookiePriority::COOKIE_PRIORITY_HIGH),
      CanonicalCookie("SAPISID", "vAlUe2", "google.com", "/", time_now,
                      time_now, expiration_time, /*is_secure=*/false,
                      /*is_http_only=*/true, net::CookieSameSite::LAX_MODE,
                      net::CookiePriority::COOKIE_PRIORITY_HIGH),
      CanonicalCookie("HSID", "vAlUe4", "", "/", time_now, time_now, time_now,
                      /*is_secure=*/true, /*is_http_only=*/true,
                      net::CookieSameSite::STRICT_MODE,
                      net::CookiePriority::COOKIE_PRIORITY_HIGH)};

  EXPECT_EQ((int)result.cookies().size(), 3);

  EXPECT_TRUE(result.cookies()[0].IsEquivalent(cookies[0]));
  EXPECT_TRUE(result.cookies()[1].IsEquivalent(cookies[1]));
  EXPECT_TRUE(result.cookies()[2].IsEquivalent(cookies[2]));

  EXPECT_FALSE(result.cookies()[0].IsExpired(base::Time::Now()));
  EXPECT_FALSE(result.cookies()[1].IsExpired(base::Time::Now()));
  EXPECT_TRUE(result.cookies()[2].IsExpired(base::Time::Now()));

  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::IsDomainCookie, Eq(true)),
                  Property(&CanonicalCookie::IsHostCookie, Eq(true)),
                  Property(&CanonicalCookie::IsDomainCookie, Eq(false))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true)),
                          Property(&CanonicalCookie::IsCanonical, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsHttpOnly, Eq(false)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(true)),
                          Property(&CanonicalCookie::IsHttpOnly, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::IsSecure, Eq(true)),
                          Property(&CanonicalCookie::IsSecure, Eq(false)),
                          Property(&CanonicalCookie::IsSecure, Eq(true))));
  EXPECT_THAT(result.cookies(),
              ElementsAre(Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::UNSPECIFIED)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::LAX_MODE)),
                          Property(&CanonicalCookie::SameSite,
                                   Eq(net::CookieSameSite::STRICT_MODE))));
  EXPECT_THAT(
      result.cookies(),
      ElementsAre(Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH)),
                  Property(&CanonicalCookie::Priority,
                           Eq(net::CookiePriority::COOKIE_PRIORITY_HIGH))));

  EXPECT_THAT(result.cookies()[0].CreationDate().ToDoubleT(),
              DoubleNear(now, 0.5));
  EXPECT_THAT(result.cookies()[0].LastAccessDate().ToDoubleT(),
              DoubleNear(now, 0.5));
  EXPECT_THAT(result.cookies()[0].ExpiryDate().ToDoubleT(),
              DoubleNear(expiration, 0.5));
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
