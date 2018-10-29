// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include <limits.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace cors {
namespace {

using CORSTest = testing::Test;

TEST_F(CORSTest, CheckAccessDetectsInvalidResponse) {
  base::Optional<CORSErrorStatus> error_status =
      CheckAccess(GURL(), 0 /* response_status_code */,
                  base::nullopt /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, url::Origin());
  ASSERT_TRUE(error_status);
  EXPECT_EQ(mojom::CORSError::kInvalidResponse, error_status->cors_error);
}

// Tests if CheckAccess detects kWildcardOriginNotAllowed error correctly.
TEST_F(CORSTest, CheckAccessDetectsWildcardOriginNotAllowed) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;
  const std::string allow_all_header("*");

  // Access-Control-Allow-Origin '*' works.
  base::Optional<CORSErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  allow_all_header /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, origin);
  EXPECT_FALSE(error1);

  // Access-Control-Allow-Origin '*' should not be allowed if credentials mode
  // is kInclude.
  base::Optional<CORSErrorStatus> error2 =
      CheckAccess(response_url, response_status_code,
                  allow_all_header /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kInclude, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kWildcardOriginNotAllowed, error2->cors_error);
}

// Tests if CheckAccess detects kMissingAllowOriginHeader error correctly.
TEST_F(CORSTest, CheckAccessDetectsMissingAllowOriginHeader) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  // Access-Control-Allow-Origin is missed.
  base::Optional<CORSErrorStatus> error =
      CheckAccess(response_url, response_status_code,
                  base::nullopt /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error);
  EXPECT_EQ(mojom::CORSError::kMissingAllowOriginHeader, error->cors_error);
}

// Tests if CheckAccess detects kMultipleAllowOriginValues error
// correctly.
TEST_F(CORSTest, CheckAccessDetectsMultipleAllowOriginValues) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  const std::string space_separated_multiple_origins(
      "http://example.com http://another.example.com");
  base::Optional<CORSErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  space_separated_multiple_origins /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error1);
  EXPECT_EQ(mojom::CORSError::kMultipleAllowOriginValues, error1->cors_error);

  const std::string comma_separated_multiple_origins(
      "http://example.com,http://another.example.com");
  base::Optional<CORSErrorStatus> error2 =
      CheckAccess(response_url, response_status_code,
                  comma_separated_multiple_origins /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kMultipleAllowOriginValues, error2->cors_error);
}

// Tests if CheckAccess detects kInvalidAllowOriginValue error correctly.
TEST_F(CORSTest, CheckAccessDetectsInvalidAllowOriginValue) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CORSErrorStatus> error =
      CheckAccess(response_url, response_status_code,
                  std::string("invalid.origin") /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error);
  EXPECT_EQ(mojom::CORSError::kInvalidAllowOriginValue, error->cors_error);
  EXPECT_EQ("invalid.origin", error->failed_parameter);
}

// Tests if CheckAccess detects kAllowOriginMismatch error correctly.
TEST_F(CORSTest, CheckAccessDetectsAllowOriginMismatch) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CORSErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  origin.Serialize() /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_FALSE(error1);

  base::Optional<CORSErrorStatus> error2 = CheckAccess(
      response_url, response_status_code,
      std::string("http://not.google.com") /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kAllowOriginMismatch, error2->cors_error);
  EXPECT_EQ("http://not.google.com", error2->failed_parameter);

  // Allow "null" value to match serialized unique origins.
  const std::string null_string("null");
  const url::Origin null_origin;
  EXPECT_EQ(null_string, null_origin.Serialize());

  base::Optional<CORSErrorStatus> error3 = CheckAccess(
      response_url, response_status_code, null_string /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::FetchCredentialsMode::kOmit, null_origin);
  EXPECT_FALSE(error3);
}

// Tests if CheckAccess detects kInvalidAllowCredentials error correctly.
TEST_F(CORSTest, CheckAccessDetectsInvalidAllowCredential) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CORSErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  origin.Serialize() /* allow_origin_header */,
                  std::string("true") /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kInclude, origin);
  ASSERT_FALSE(error1);

  base::Optional<CORSErrorStatus> error2 =
      CheckAccess(response_url, response_status_code,
                  origin.Serialize() /* allow_origin_header */,
                  std::string("fuga") /* allow_credentials_header */,
                  network::mojom::FetchCredentialsMode::kInclude, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kInvalidAllowCredentials, error2->cors_error);
  EXPECT_EQ("fuga", error2->failed_parameter);
}

// Tests if CheckRedirectLocation detects kCORSDisabledScheme and
// kRedirectContainsCredentials errors correctly.
TEST_F(CORSTest, CheckRedirectLocation) {
  struct TestCase {
    GURL url;
    mojom::FetchRequestMode request_mode;
    bool cors_flag;
    bool tainted;
    base::Optional<CORSErrorStatus> expectation;
  };

  const auto kCORS = mojom::FetchRequestMode::kCORS;
  const auto kCORSWithForcedPreflight =
      mojom::FetchRequestMode::kCORSWithForcedPreflight;
  const auto kNoCORS = mojom::FetchRequestMode::kNoCORS;

  const url::Origin origin = url::Origin::Create(GURL("http://example.com/"));
  const GURL same_origin_url("http://example.com/");
  const GURL cross_origin_url("http://example2.com/");
  const GURL data_url("data:,Hello");
  const GURL same_origin_url_with_user("http://yukari@example.com/");
  const GURL same_origin_url_with_pass("http://:tamura@example.com/");
  const GURL cross_origin_url_with_user("http://yukari@example2.com/");
  const GURL cross_origin_url_with_pass("http://:tamura@example2.com/");
  const auto ok = base::nullopt;
  const CORSErrorStatus kCORSDisabledScheme(
      mojom::CORSError::kCORSDisabledScheme);
  const CORSErrorStatus kRedirectContainsCredentials(
      mojom::CORSError::kRedirectContainsCredentials);

  TestCase cases[] = {
      // "cors", no credentials information
      {same_origin_url, kCORS, false, false, ok},
      {cross_origin_url, kCORS, false, false, ok},
      {data_url, kCORS, false, false, ok},
      {same_origin_url, kCORS, true, false, ok},
      {cross_origin_url, kCORS, true, false, ok},
      {data_url, kCORS, true, false, ok},
      {same_origin_url, kCORS, false, true, ok},
      {cross_origin_url, kCORS, false, true, ok},
      {data_url, kCORS, false, true, ok},
      {same_origin_url, kCORS, true, true, ok},
      {cross_origin_url, kCORS, true, true, ok},
      {data_url, kCORS, true, true, ok},

      // "cors" with forced preflight, no credentials information
      {same_origin_url, kCORSWithForcedPreflight, false, false, ok},
      {cross_origin_url, kCORSWithForcedPreflight, false, false, ok},
      {data_url, kCORSWithForcedPreflight, false, false, ok},
      {same_origin_url, kCORSWithForcedPreflight, true, false, ok},
      {cross_origin_url, kCORSWithForcedPreflight, true, false, ok},
      {data_url, kCORSWithForcedPreflight, true, false, ok},
      {same_origin_url, kCORSWithForcedPreflight, false, true, ok},
      {cross_origin_url, kCORSWithForcedPreflight, false, true, ok},
      {data_url, kCORSWithForcedPreflight, false, true, ok},
      {same_origin_url, kCORSWithForcedPreflight, true, true, ok},
      {cross_origin_url, kCORSWithForcedPreflight, true, true, ok},
      {data_url, kCORSWithForcedPreflight, true, true, ok},

      // "no-cors", no credentials information
      {same_origin_url, kNoCORS, false, false, ok},
      {cross_origin_url, kNoCORS, false, false, ok},
      {data_url, kNoCORS, false, false, ok},
      {same_origin_url, kNoCORS, false, true, ok},
      {cross_origin_url, kNoCORS, false, true, ok},
      {data_url, kNoCORS, false, true, ok},

      // with credentials information (same origin)
      {same_origin_url_with_user, kCORS, false, false, ok},
      {same_origin_url_with_user, kCORS, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kCORS, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kNoCORS, false, false, ok},
      {same_origin_url_with_user, kNoCORS, false, true, ok},
      {same_origin_url_with_pass, kCORS, false, false, ok},
      {same_origin_url_with_pass, kCORS, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kCORS, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kNoCORS, false, false, ok},
      {same_origin_url_with_pass, kNoCORS, false, true, ok},

      // with credentials information (cross origin)
      {cross_origin_url_with_user, kCORS, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCORS, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCORS, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kNoCORS, false, true, ok},
      {cross_origin_url_with_user, kNoCORS, false, false, ok},
      {cross_origin_url_with_pass, kCORS, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCORS, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCORS, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kNoCORS, false, true, ok},
      {cross_origin_url_with_pass, kNoCORS, false, false, ok},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: " << test.url
                 << ", request mode: " << test.request_mode
                 << ", origin: " << origin << ", cors_flag: " << test.cors_flag
                 << ", tainted: " << test.tainted);

    EXPECT_EQ(test.expectation,
              CheckRedirectLocation(test.url, test.request_mode, origin,
                                    test.cors_flag, test.tainted));
  }
}

TEST_F(CORSTest, CheckPreflightDetectsErrors) {
  EXPECT_FALSE(CheckPreflight(200));
  EXPECT_FALSE(CheckPreflight(299));

  base::Optional<mojom::CORSError> error1 = CheckPreflight(300);
  ASSERT_TRUE(error1);
  EXPECT_EQ(mojom::CORSError::kPreflightInvalidStatus, *error1);

  EXPECT_FALSE(CheckExternalPreflight(std::string("true")));

  base::Optional<CORSErrorStatus> error2 =
      CheckExternalPreflight(base::nullopt);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CORSError::kPreflightMissingAllowExternal,
            error2->cors_error);
  EXPECT_EQ("", error2->failed_parameter);

  base::Optional<CORSErrorStatus> error3 =
      CheckExternalPreflight(std::string("TRUE"));
  ASSERT_TRUE(error3);
  EXPECT_EQ(mojom::CORSError::kPreflightInvalidAllowExternal,
            error3->cors_error);
  EXPECT_EQ("TRUE", error3->failed_parameter);
}

TEST_F(CORSTest, CalculateResponseTainting) {
  using mojom::FetchResponseType;
  using mojom::FetchRequestMode;

  const GURL same_origin_url("https://example.com/");
  const GURL cross_origin_url("https://example2.com/");
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const base::Optional<url::Origin> no_origin;

  // CORS flag is false, same-origin request
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kSameOrigin, origin, false));
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNoCORS, origin, false));
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(same_origin_url, FetchRequestMode::kCORS,
                                      origin, false));
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCORSWithForcedPreflight,
                origin, false));
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNavigate, origin, false));

  // CORS flag is false, cross-origin request
  EXPECT_EQ(FetchResponseType::kOpaque,
            CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNoCORS, origin, false));
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNavigate, origin, false));

  // CORS flag is true, same-origin request
  EXPECT_EQ(FetchResponseType::kCORS,
            CalculateResponseTainting(same_origin_url, FetchRequestMode::kCORS,
                                      origin, true));
  EXPECT_EQ(FetchResponseType::kCORS,
            CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kCORSWithForcedPreflight,
                origin, true));

  // CORS flag is true, cross-origin request
  EXPECT_EQ(FetchResponseType::kCORS,
            CalculateResponseTainting(cross_origin_url, FetchRequestMode::kCORS,
                                      origin, true));
  EXPECT_EQ(FetchResponseType::kCORS,
            CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kCORSWithForcedPreflight,
                origin, true));

  // Origin is not provided.
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                same_origin_url, FetchRequestMode::kNoCORS, no_origin, false));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      CalculateResponseTainting(same_origin_url, FetchRequestMode::kNavigate,
                                no_origin, false));
  EXPECT_EQ(FetchResponseType::kBasic,
            CalculateResponseTainting(
                cross_origin_url, FetchRequestMode::kNoCORS, no_origin, false));
  EXPECT_EQ(
      FetchResponseType::kBasic,
      CalculateResponseTainting(cross_origin_url, FetchRequestMode::kNavigate,
                                no_origin, false));
}

TEST_F(CORSTest, SafelistedMethod) {
  // Method check should be case-insensitive.
  EXPECT_TRUE(IsCORSSafelistedMethod("get"));
  EXPECT_TRUE(IsCORSSafelistedMethod("Get"));
  EXPECT_TRUE(IsCORSSafelistedMethod("GET"));
  EXPECT_TRUE(IsCORSSafelistedMethod("HEAD"));
  EXPECT_TRUE(IsCORSSafelistedMethod("POST"));
  EXPECT_FALSE(IsCORSSafelistedMethod("OPTIONS"));
}

TEST_F(CORSTest, SafelistedHeader) {
  // See SafelistedAccept/AcceptLanguage/ContentLanguage/ContentType also.

  EXPECT_TRUE(IsCORSSafelistedHeader("accept", "foo"));
  EXPECT_FALSE(IsCORSSafelistedHeader("foo", "bar"));
  EXPECT_FALSE(IsCORSSafelistedHeader("user-agent", "foo"));
}

TEST_F(CORSTest, SafelistedAccept) {
  EXPECT_TRUE(IsCORSSafelistedHeader("accept", "text/html"));
  EXPECT_TRUE(IsCORSSafelistedHeader("AccepT", "text/html"));

  constexpr char kAllowed[] =
      "\t !#$%&'*+,-./0123456789;="
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~";
  for (int i = CHAR_MIN; i <= CHAR_MAX; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(i);
    // 1 for the trailing null character.
    auto* end = kAllowed + base::size(kAllowed) - 1;
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCORSSafelistedHeader("accept", std::string(1, c)));
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCORSSafelistedHeader("AccepT", std::string(1, c)));
  }

  EXPECT_TRUE(IsCORSSafelistedHeader("accept", std::string(128, 'a')));
  EXPECT_FALSE(IsCORSSafelistedHeader("accept", std::string(129, 'a')));
  EXPECT_TRUE(IsCORSSafelistedHeader("AccepT", std::string(128, 'a')));
  EXPECT_FALSE(IsCORSSafelistedHeader("AccepT", std::string(129, 'a')));
}

TEST_F(CORSTest, SafelistedAcceptLanguage) {
  EXPECT_TRUE(IsCORSSafelistedHeader("accept-language", "en,ja"));
  EXPECT_TRUE(IsCORSSafelistedHeader("aCcEPT-lAngUAge", "en,ja"));

  constexpr char kAllowed[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz *,-.;=";
  for (int i = CHAR_MIN; i <= CHAR_MAX; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(i);
    // 1 for the trailing null character.
    auto* end = kAllowed + base::size(kAllowed) - 1;
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCORSSafelistedHeader("aCcEPT-lAngUAge", std::string(1, c)));
  }
  EXPECT_TRUE(IsCORSSafelistedHeader("accept-language", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCORSSafelistedHeader("accept-language", std::string(129, 'a')));
  EXPECT_TRUE(IsCORSSafelistedHeader("aCcEPT-lAngUAge", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCORSSafelistedHeader("aCcEPT-lAngUAge", std::string(129, 'a')));
}

TEST_F(CORSTest, SafelistedContentLanguage) {
  EXPECT_TRUE(IsCORSSafelistedHeader("content-language", "en,ja"));
  EXPECT_TRUE(IsCORSSafelistedHeader("cONTent-LANguaGe", "en,ja"));

  constexpr char kAllowed[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz *,-.;=";
  for (int i = CHAR_MIN; i <= CHAR_MAX; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(i);
    // 1 for the trailing null character.
    auto* end = kAllowed + base::size(kAllowed) - 1;
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCORSSafelistedHeader("content-language", std::string(1, c)));
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCORSSafelistedHeader("cONTent-LANguaGe", std::string(1, c)));
  }
  EXPECT_TRUE(
      IsCORSSafelistedHeader("content-language", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCORSSafelistedHeader("content-language", std::string(129, 'a')));
  EXPECT_TRUE(
      IsCORSSafelistedHeader("cONTent-LANguaGe", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCORSSafelistedHeader("cONTent-LANguaGe", std::string(129, 'a')));
}

TEST_F(CORSTest, SafelistedContentType) {
  EXPECT_TRUE(IsCORSSafelistedHeader("content-type", "text/plain"));
  EXPECT_TRUE(IsCORSSafelistedHeader("CoNtEnt-TyPE", "text/plain"));
  EXPECT_TRUE(
      IsCORSSafelistedHeader("content-type", "text/plain; charset=utf-8"));
  EXPECT_TRUE(
      IsCORSSafelistedHeader("content-type", "  text/plain ; charset=UTF-8"));
  EXPECT_TRUE(
      IsCORSSafelistedHeader("content-type", "text/plain; param=BOGUS"));
  EXPECT_TRUE(IsCORSSafelistedHeader("content-type",
                                     "application/x-www-form-urlencoded"));
  EXPECT_TRUE(IsCORSSafelistedHeader("content-type", "multipart/form-data"));

  EXPECT_TRUE(IsCORSSafelistedHeader("content-type", "Text/plain"));
  EXPECT_TRUE(IsCORSSafelistedHeader("content-type", "tEXT/PLAIN"));
  EXPECT_FALSE(IsCORSSafelistedHeader("content-type", "text/html"));
  EXPECT_FALSE(IsCORSSafelistedHeader("CoNtEnt-TyPE", "text/html"));

  EXPECT_FALSE(IsCORSSafelistedHeader("content-type", "image/png"));
  EXPECT_FALSE(IsCORSSafelistedHeader("CoNtEnt-TyPE", "image/png"));
  EXPECT_TRUE(IsCORSSafelistedHeader(
      "content-type", "text/plain; charset=" + std::string(108, 'a')));
  EXPECT_TRUE(IsCORSSafelistedHeader(
      "cONTent-tYPE", "text/plain; charset=" + std::string(108, 'a')));
  EXPECT_FALSE(IsCORSSafelistedHeader(
      "content-type", "text/plain; charset=" + std::string(109, 'a')));
  EXPECT_FALSE(IsCORSSafelistedHeader(
      "cONTent-tYPE", "text/plain; charset=" + std::string(109, 'a')));
}

TEST_F(CORSTest, CheckCORSClientHintsSafelist) {
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", ""));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "abc"));
  EXPECT_TRUE(IsCORSSafelistedHeader("device-memory", "1.25"));
  EXPECT_TRUE(IsCORSSafelistedHeader("DEVICE-memory", "1.25"));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "1.25-2.5"));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "-1.25"));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "1e2"));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "inf"));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "-2.3"));
  EXPECT_FALSE(IsCORSSafelistedHeader("device-memory", "NaN"));
  EXPECT_FALSE(IsCORSSafelistedHeader("DEVICE-memory", "1.25.3"));
  EXPECT_FALSE(IsCORSSafelistedHeader("DEVICE-memory", "1."));
  EXPECT_FALSE(IsCORSSafelistedHeader("DEVICE-memory", ".1"));
  EXPECT_FALSE(IsCORSSafelistedHeader("DEVICE-memory", "."));
  EXPECT_TRUE(IsCORSSafelistedHeader("DEVICE-memory", "1"));

  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", ""));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "abc"));
  EXPECT_TRUE(IsCORSSafelistedHeader("dpr", "1.25"));
  EXPECT_TRUE(IsCORSSafelistedHeader("Dpr", "1.25"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "1.25-2.5"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "-1.25"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "1e2"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "inf"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "-2.3"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "NaN"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "1.25.3"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "1."));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", ".1"));
  EXPECT_FALSE(IsCORSSafelistedHeader("dpr", "."));
  EXPECT_TRUE(IsCORSSafelistedHeader("dpr", "1"));

  EXPECT_FALSE(IsCORSSafelistedHeader("width", ""));
  EXPECT_FALSE(IsCORSSafelistedHeader("width", "abc"));
  EXPECT_TRUE(IsCORSSafelistedHeader("width", "125"));
  EXPECT_TRUE(IsCORSSafelistedHeader("width", "1"));
  EXPECT_TRUE(IsCORSSafelistedHeader("WIDTH", "125"));
  EXPECT_FALSE(IsCORSSafelistedHeader("width", "125.2"));
  EXPECT_FALSE(IsCORSSafelistedHeader("width", "-125"));
  EXPECT_TRUE(IsCORSSafelistedHeader("width", "2147483648"));

  EXPECT_FALSE(IsCORSSafelistedHeader("viewport-width", ""));
  EXPECT_FALSE(IsCORSSafelistedHeader("viewport-width", "abc"));
  EXPECT_TRUE(IsCORSSafelistedHeader("viewport-width", "125"));
  EXPECT_TRUE(IsCORSSafelistedHeader("viewport-width", "1"));
  EXPECT_TRUE(IsCORSSafelistedHeader("viewport-Width", "125"));
  EXPECT_FALSE(IsCORSSafelistedHeader("viewport-width", "125.2"));
  EXPECT_TRUE(IsCORSSafelistedHeader("viewport-width", "2147483648"));
}

TEST_F(CORSTest, CORSUnsafeRequestHeaderNames) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(CORSUnsafeRequestHeaderNames({}), List({}));

  // Some headers are safelisted.
  EXPECT_EQ(CORSUnsafeRequestHeaderNames({{"content-type", "text/plain"},
                                          {"dpr", "12345"},
                                          {"aCCept", "en,ja"},
                                          {"accept-charset", "utf-8"},
                                          {"uSer-Agent", "foo"},
                                          {"hogE", "fuga"}}),
            List({"accept-charset", "user-agent", "hoge"}));

  // All headers are not safelisted.
  EXPECT_EQ(
      CORSUnsafeRequestHeaderNames({{"content-type", "text/html"},
                                    {"dpr", "123-45"},
                                    {"aCCept", "en,ja"},
                                    {"accept-charset", "utf-8"},
                                    {"uSer-Agent", "foo"},
                                    {"hogE", "fuga"}}),
      List({"content-type", "dpr", "accept-charset", "user-agent", "hoge"}));

  // |safelistValueSize| is 1024.
  EXPECT_EQ(
      CORSUnsafeRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(108, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(126, '1')},
           {"hogE", "fuga"}}),
      List({"hoge"}));

  // |safelistValueSize| is 1025.
  EXPECT_EQ(
      CORSUnsafeRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(108, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(127, '1')},
           {"hogE", "fuga"}}),
      List({"hoge", "content-type", "accept", "accept-language",
            "content-language", "dpr", "device-memory", "save-data",
            "viewport-width", "width"}));

  // |safelistValueSize| is 897 because "content-type" is not safelisted.
  EXPECT_EQ(
      CORSUnsafeRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(128, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(127, '1')},
           {"hogE", "fuga"}}),
      List({"content-type", "hoge"}));
}

TEST_F(CORSTest, CORSUnsafeNotForbiddenRequestHeaderNames) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames({}, false /* is_revalidating */),
      List({}));

  // "user-agent" is NOT forbidden per spec, but forbidden in Chromium.
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames({{"content-type", "text/plain"},
                                                {"dpr", "12345"},
                                                {"aCCept", "en,ja"},
                                                {"accept-charset", "utf-8"},
                                                {"uSer-Agent", "foo"},
                                                {"hogE", "fuga"}},
                                               false /* is_revalidating */),
      List({"hoge"}));

  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames({{"content-type", "text/html"},
                                                {"dpr", "123-45"},
                                                {"aCCept", "en,ja"},
                                                {"accept-charset", "utf-8"},
                                                {"hogE", "fuga"}},
                                               false /* is_revalidating */),
      List({"content-type", "dpr", "hoge"}));

  // |safelistValueSize| is 1024.
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(108, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(126, '1')},
           {"accept-charset", "utf-8"},
           {"hogE", "fuga"}},
          false /* is_revalidating */),
      List({"hoge"}));

  // |safelistValueSize| is 1025.
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(108, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(127, '1')},
           {"accept-charset", "utf-8"},
           {"hogE", "fuga"}},
          false /* is_revalidating */),
      List({"hoge", "content-type", "accept", "accept-language",
            "content-language", "dpr", "device-memory", "save-data",
            "viewport-width", "width"}));

  // |safelistValueSize| is 897 because "content-type" is not safelisted.
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(128, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(127, '1')},
           {"accept-charset", "utf-8"},
           {"hogE", "fuga"}},
          false /* is_revalidating */),
      List({"content-type", "hoge"}));
}

TEST_F(CORSTest, CORSUnsafeNotForbiddenRequestHeaderNamesWithRevalidating) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames({}, true /* is_revalidating */),
      List({}));

  // These three headers will be ignored.
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames({{"If-MODifIED-since", "x"},
                                                {"iF-nONE-MATCh", "y"},
                                                {"CACHE-ContrOl", "z"}},
                                               true /* is_revalidating */),
      List({}));

  // Without is_revalidating set, these three headers will not be safelisted.
  EXPECT_EQ(
      CORSUnsafeNotForbiddenRequestHeaderNames({{"If-MODifIED-since", "x"},
                                                {"iF-nONE-MATCh", "y"},
                                                {"CACHE-ContrOl", "z"}},
                                               false /* is_revalidating */),
      List({"if-modified-since", "if-none-match", "cache-control"}));
}

}  // namespace
}  // namespace cors
}  // namespace network
