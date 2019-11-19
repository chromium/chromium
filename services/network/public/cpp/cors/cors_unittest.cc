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

using CorsTest = testing::Test;

TEST_F(CorsTest, CheckAccessDetectsInvalidResponse) {
  base::Optional<CorsErrorStatus> error_status =
      CheckAccess(GURL(), 0 /* response_status_code */,
                  base::nullopt /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, url::Origin());
  ASSERT_TRUE(error_status);
  EXPECT_EQ(mojom::CorsError::kInvalidResponse, error_status->cors_error);
}

// Tests if CheckAccess detects kWildcardOriginNotAllowed error correctly.
TEST_F(CorsTest, CheckAccessDetectsWildcardOriginNotAllowed) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;
  const std::string allow_all_header("*");

  // Access-Control-Allow-Origin '*' works.
  base::Optional<CorsErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  allow_all_header /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  EXPECT_FALSE(error1);

  // Access-Control-Allow-Origin '*' should not be allowed if credentials mode
  // is kInclude.
  base::Optional<CorsErrorStatus> error2 =
      CheckAccess(response_url, response_status_code,
                  allow_all_header /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kInclude, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CorsError::kWildcardOriginNotAllowed, error2->cors_error);
}

// Tests if CheckAccess detects kMissingAllowOriginHeader error correctly.
TEST_F(CorsTest, CheckAccessDetectsMissingAllowOriginHeader) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  // Access-Control-Allow-Origin is missed.
  base::Optional<CorsErrorStatus> error =
      CheckAccess(response_url, response_status_code,
                  base::nullopt /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_TRUE(error);
  EXPECT_EQ(mojom::CorsError::kMissingAllowOriginHeader, error->cors_error);
}

// Tests if CheckAccess detects kMultipleAllowOriginValues error
// correctly.
TEST_F(CorsTest, CheckAccessDetectsMultipleAllowOriginValues) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  const std::string space_separated_multiple_origins(
      "http://example.com http://another.example.com");
  base::Optional<CorsErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  space_separated_multiple_origins /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_TRUE(error1);
  EXPECT_EQ(mojom::CorsError::kMultipleAllowOriginValues, error1->cors_error);

  const std::string comma_separated_multiple_origins(
      "http://example.com,http://another.example.com");
  base::Optional<CorsErrorStatus> error2 =
      CheckAccess(response_url, response_status_code,
                  comma_separated_multiple_origins /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CorsError::kMultipleAllowOriginValues, error2->cors_error);
}

// Tests if CheckAccess detects kInvalidAllowOriginValue error correctly.
TEST_F(CorsTest, CheckAccessDetectsInvalidAllowOriginValue) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CorsErrorStatus> error =
      CheckAccess(response_url, response_status_code,
                  std::string("invalid.origin") /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_TRUE(error);
  EXPECT_EQ(mojom::CorsError::kInvalidAllowOriginValue, error->cors_error);
  EXPECT_EQ("invalid.origin", error->failed_parameter);
}

// Tests if CheckAccess detects kAllowOriginMismatch error correctly.
TEST_F(CorsTest, CheckAccessDetectsAllowOriginMismatch) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CorsErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  origin.Serialize() /* allow_origin_header */,
                  base::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(error1);

  base::Optional<CorsErrorStatus> error2 = CheckAccess(
      response_url, response_status_code,
      std::string("http://not.google.com") /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CorsError::kAllowOriginMismatch, error2->cors_error);
  EXPECT_EQ("http://not.google.com", error2->failed_parameter);

  // Allow "null" value to match serialized unique origins.
  const std::string null_string("null");
  const url::Origin null_origin;
  EXPECT_EQ(null_string, null_origin.Serialize());

  base::Optional<CorsErrorStatus> error3 = CheckAccess(
      response_url, response_status_code, null_string /* allow_origin_header */,
      base::nullopt /* allow_credentials_header */,
      network::mojom::CredentialsMode::kOmit, null_origin);
  EXPECT_FALSE(error3);
}

// Tests if CheckAccess detects kInvalidAllowCredentials error correctly.
TEST_F(CorsTest, CheckAccessDetectsInvalidAllowCredential) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const int response_status_code = 200;

  base::Optional<CorsErrorStatus> error1 =
      CheckAccess(response_url, response_status_code,
                  origin.Serialize() /* allow_origin_header */,
                  std::string("true") /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kInclude, origin);
  ASSERT_FALSE(error1);

  base::Optional<CorsErrorStatus> error2 =
      CheckAccess(response_url, response_status_code,
                  origin.Serialize() /* allow_origin_header */,
                  std::string("fuga") /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kInclude, origin);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CorsError::kInvalidAllowCredentials, error2->cors_error);
  EXPECT_EQ("fuga", error2->failed_parameter);
}

// Tests if CheckRedirectLocation detects kCorsDisabledScheme and
// kRedirectContainsCredentials errors correctly.
TEST_F(CorsTest, CheckRedirectLocation) {
  struct TestCase {
    GURL url;
    mojom::RequestMode request_mode;
    bool cors_flag;
    bool tainted;
    base::Optional<CorsErrorStatus> expectation;
  };

  const auto kCors = mojom::RequestMode::kCors;
  const auto kCorsWithForcedPreflight =
      mojom::RequestMode::kCorsWithForcedPreflight;
  const auto kNoCors = mojom::RequestMode::kNoCors;

  const url::Origin origin = url::Origin::Create(GURL("http://example.com/"));
  const GURL same_origin_url("http://example.com/");
  const GURL cross_origin_url("http://example2.com/");
  const GURL data_url("data:,Hello");
  const GURL same_origin_url_with_user("http://yukari@example.com/");
  const GURL same_origin_url_with_pass("http://:tamura@example.com/");
  const GURL cross_origin_url_with_user("http://yukari@example2.com/");
  const GURL cross_origin_url_with_pass("http://:tamura@example2.com/");
  const auto ok = base::nullopt;
  const CorsErrorStatus kCorsDisabledScheme(
      mojom::CorsError::kCorsDisabledScheme);
  const CorsErrorStatus kRedirectContainsCredentials(
      mojom::CorsError::kRedirectContainsCredentials);

  TestCase cases[] = {
      // "cors", no credentials information
      {same_origin_url, kCors, false, false, ok},
      {cross_origin_url, kCors, false, false, ok},
      {data_url, kCors, false, false, ok},
      {same_origin_url, kCors, true, false, ok},
      {cross_origin_url, kCors, true, false, ok},
      {data_url, kCors, true, false, ok},
      {same_origin_url, kCors, false, true, ok},
      {cross_origin_url, kCors, false, true, ok},
      {data_url, kCors, false, true, ok},
      {same_origin_url, kCors, true, true, ok},
      {cross_origin_url, kCors, true, true, ok},
      {data_url, kCors, true, true, ok},

      // "cors" with forced preflight, no credentials information
      {same_origin_url, kCorsWithForcedPreflight, false, false, ok},
      {cross_origin_url, kCorsWithForcedPreflight, false, false, ok},
      {data_url, kCorsWithForcedPreflight, false, false, ok},
      {same_origin_url, kCorsWithForcedPreflight, true, false, ok},
      {cross_origin_url, kCorsWithForcedPreflight, true, false, ok},
      {data_url, kCorsWithForcedPreflight, true, false, ok},
      {same_origin_url, kCorsWithForcedPreflight, false, true, ok},
      {cross_origin_url, kCorsWithForcedPreflight, false, true, ok},
      {data_url, kCorsWithForcedPreflight, false, true, ok},
      {same_origin_url, kCorsWithForcedPreflight, true, true, ok},
      {cross_origin_url, kCorsWithForcedPreflight, true, true, ok},
      {data_url, kCorsWithForcedPreflight, true, true, ok},

      // "no-cors", no credentials information
      {same_origin_url, kNoCors, false, false, ok},
      {cross_origin_url, kNoCors, false, false, ok},
      {data_url, kNoCors, false, false, ok},
      {same_origin_url, kNoCors, false, true, ok},
      {cross_origin_url, kNoCors, false, true, ok},
      {data_url, kNoCors, false, true, ok},

      // with credentials information (same origin)
      {same_origin_url_with_user, kCors, false, false, ok},
      {same_origin_url_with_user, kCors, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kCors, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_user, kNoCors, false, false, ok},
      {same_origin_url_with_user, kNoCors, false, true, ok},
      {same_origin_url_with_pass, kCors, false, false, ok},
      {same_origin_url_with_pass, kCors, true, false,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kCors, true, true,
       kRedirectContainsCredentials},
      {same_origin_url_with_pass, kNoCors, false, false, ok},
      {same_origin_url_with_pass, kNoCors, false, true, ok},

      // with credentials information (cross origin)
      {cross_origin_url_with_user, kCors, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCors, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kCors, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_user, kNoCors, false, true, ok},
      {cross_origin_url_with_user, kNoCors, false, false, ok},
      {cross_origin_url_with_pass, kCors, false, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCors, true, false,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kCors, true, true,
       kRedirectContainsCredentials},
      {cross_origin_url_with_pass, kNoCors, false, true, ok},
      {cross_origin_url_with_pass, kNoCors, false, false, ok},
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

TEST_F(CorsTest, CheckPreflightDetectsErrors) {
  EXPECT_FALSE(CheckPreflight(200));
  EXPECT_FALSE(CheckPreflight(299));

  base::Optional<mojom::CorsError> error1 = CheckPreflight(300);
  ASSERT_TRUE(error1);
  EXPECT_EQ(mojom::CorsError::kPreflightInvalidStatus, *error1);

  EXPECT_FALSE(CheckExternalPreflight(std::string("true")));

  base::Optional<CorsErrorStatus> error2 =
      CheckExternalPreflight(base::nullopt);
  ASSERT_TRUE(error2);
  EXPECT_EQ(mojom::CorsError::kPreflightMissingAllowExternal,
            error2->cors_error);
  EXPECT_EQ("", error2->failed_parameter);

  base::Optional<CorsErrorStatus> error3 =
      CheckExternalPreflight(std::string("TRUE"));
  ASSERT_TRUE(error3);
  EXPECT_EQ(mojom::CorsError::kPreflightInvalidAllowExternal,
            error3->cors_error);
  EXPECT_EQ("TRUE", error3->failed_parameter);
}

TEST_F(CorsTest, SafelistedMethod) {
  // Method check should be case-insensitive.
  EXPECT_TRUE(IsCorsSafelistedMethod("get"));
  EXPECT_TRUE(IsCorsSafelistedMethod("Get"));
  EXPECT_TRUE(IsCorsSafelistedMethod("GET"));
  EXPECT_TRUE(IsCorsSafelistedMethod("HEAD"));
  EXPECT_TRUE(IsCorsSafelistedMethod("POST"));
  EXPECT_FALSE(IsCorsSafelistedMethod("OPTIONS"));
}

TEST_F(CorsTest, SafelistedHeader) {
  // See SafelistedAccept/AcceptLanguage/ContentLanguage/ContentType also.

  EXPECT_TRUE(IsCorsSafelistedHeader("accept", "foo"));
  EXPECT_FALSE(IsCorsSafelistedHeader("foo", "bar"));
  EXPECT_FALSE(IsCorsSafelistedHeader("user-agent", "foo"));
}

TEST_F(CorsTest, SafelistedAccept) {
  EXPECT_TRUE(IsCorsSafelistedHeader("accept", "text/html"));
  EXPECT_TRUE(IsCorsSafelistedHeader("AccepT", "text/html"));

  constexpr char kAllowed[] =
      "\t !#$%&'*+,-./0123456789;="
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~";
  for (int i = 0; i < 128; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(i);
    // 1 for the trailing null character.
    auto* end = kAllowed + base::size(kAllowed) - 1;
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCorsSafelistedHeader("accept", std::string(1, c)));
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCorsSafelistedHeader("AccepT", std::string(1, c)));
  }
  for (int i = 128; i <= 255; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(static_cast<unsigned char>(i));
    EXPECT_TRUE(IsCorsSafelistedHeader("accept", std::string(1, c)));
    EXPECT_TRUE(IsCorsSafelistedHeader("AccepT", std::string(1, c)));
  }

  EXPECT_TRUE(IsCorsSafelistedHeader("accept", std::string(128, 'a')));
  EXPECT_FALSE(IsCorsSafelistedHeader("accept", std::string(129, 'a')));
  EXPECT_TRUE(IsCorsSafelistedHeader("AccepT", std::string(128, 'a')));
  EXPECT_FALSE(IsCorsSafelistedHeader("AccepT", std::string(129, 'a')));
}

TEST_F(CorsTest, SafelistedAcceptLanguage) {
  EXPECT_TRUE(IsCorsSafelistedHeader("accept-language", "en,ja"));
  EXPECT_TRUE(IsCorsSafelistedHeader("aCcEPT-lAngUAge", "en,ja"));

  constexpr char kAllowed[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz *,-.;=";
  for (int i = CHAR_MIN; i <= CHAR_MAX; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(i);
    // 1 for the trailing null character.
    auto* end = kAllowed + base::size(kAllowed) - 1;
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCorsSafelistedHeader("aCcEPT-lAngUAge", std::string(1, c)));
  }
  EXPECT_TRUE(IsCorsSafelistedHeader("accept-language", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCorsSafelistedHeader("accept-language", std::string(129, 'a')));
  EXPECT_TRUE(IsCorsSafelistedHeader("aCcEPT-lAngUAge", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCorsSafelistedHeader("aCcEPT-lAngUAge", std::string(129, 'a')));
}

TEST_F(CorsTest, SafelistedSecCHLang) {
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-Lang", "\"en\", \"de\""));

  // TODO(mkwst): Validate that `Sec-CH-Lang` is a structured header.
  // https://crbug.com/924969
}

TEST_F(CorsTest, SafelistedSecCHUA) {
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA", "\"User Agent!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Platform", "\"Platform!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Arch", "\"Architecture!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Model", "\"Model!\""));

  // TODO(mkwst): Validate that `Sec-CH-UA-*` is a structured header.
  // https://crbug.com/924969
}

TEST_F(CorsTest, SafelistedContentLanguage) {
  EXPECT_TRUE(IsCorsSafelistedHeader("content-language", "en,ja"));
  EXPECT_TRUE(IsCorsSafelistedHeader("cONTent-LANguaGe", "en,ja"));

  constexpr char kAllowed[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz *,-.;=";
  for (int i = CHAR_MIN; i <= CHAR_MAX; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(i);
    // 1 for the trailing null character.
    auto* end = kAllowed + base::size(kAllowed) - 1;
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCorsSafelistedHeader("content-language", std::string(1, c)));
    EXPECT_EQ(std::find(kAllowed, end, c) != end,
              IsCorsSafelistedHeader("cONTent-LANguaGe", std::string(1, c)));
  }
  EXPECT_TRUE(
      IsCorsSafelistedHeader("content-language", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCorsSafelistedHeader("content-language", std::string(129, 'a')));
  EXPECT_TRUE(
      IsCorsSafelistedHeader("cONTent-LANguaGe", std::string(128, 'a')));
  EXPECT_FALSE(
      IsCorsSafelistedHeader("cONTent-LANguaGe", std::string(129, 'a')));
}

TEST_F(CorsTest, SafelistedContentType) {
  constexpr char kAllowed[] =
      "\t !#$%&'*+,-./0123456789;="
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~";
  for (int i = 0; i < 128; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    const char c = static_cast<char>(i);
    // 1 for the trailing null character.
    const auto* const end = kAllowed + base::size(kAllowed) - 1;
    const bool is_allowed = std::find(kAllowed, end, c) != end;
    const std::string value = std::string("text/plain; charset=") + c;

    EXPECT_EQ(is_allowed, IsCorsSafelistedHeader("content-type", value));
    EXPECT_EQ(is_allowed, IsCorsSafelistedHeader("cONtent-tYPe", value));
  }
  for (int i = 128; i <= 255; ++i) {
    SCOPED_TRACE(testing::Message() << "c = static_cast<char>(" << i << ")");
    char c = static_cast<char>(static_cast<unsigned char>(i));
    const std::string value = std::string("text/plain; charset=") + c;
    EXPECT_TRUE(IsCorsSafelistedHeader("content-type", value));
    EXPECT_TRUE(IsCorsSafelistedHeader("ConTEnt-Type", value));
  }

  EXPECT_TRUE(IsCorsSafelistedHeader("content-type", "text/plain"));
  EXPECT_TRUE(IsCorsSafelistedHeader("CoNtEnt-TyPE", "text/plain"));
  EXPECT_TRUE(
      IsCorsSafelistedHeader("content-type", "text/plain; charset=utf-8"));
  EXPECT_TRUE(
      IsCorsSafelistedHeader("content-type", "  text/plain ; charset=UTF-8"));
  EXPECT_TRUE(
      IsCorsSafelistedHeader("content-type", "text/plain; param=BOGUS"));
  EXPECT_TRUE(IsCorsSafelistedHeader("content-type",
                                     "application/x-www-form-urlencoded"));
  EXPECT_TRUE(IsCorsSafelistedHeader("content-type", "multipart/form-data"));

  EXPECT_TRUE(IsCorsSafelistedHeader("content-type", "Text/plain"));
  EXPECT_TRUE(IsCorsSafelistedHeader("content-type", "tEXT/PLAIN"));
  EXPECT_FALSE(IsCorsSafelistedHeader("content-type", "text/html"));
  EXPECT_FALSE(IsCorsSafelistedHeader("CoNtEnt-TyPE", "text/html"));

  EXPECT_FALSE(IsCorsSafelistedHeader("content-type", "image/png"));
  EXPECT_FALSE(IsCorsSafelistedHeader("CoNtEnt-TyPE", "image/png"));
  EXPECT_TRUE(IsCorsSafelistedHeader(
      "content-type", "text/plain; charset=" + std::string(108, 'a')));
  EXPECT_TRUE(IsCorsSafelistedHeader(
      "cONTent-tYPE", "text/plain; charset=" + std::string(108, 'a')));
  EXPECT_FALSE(IsCorsSafelistedHeader(
      "content-type", "text/plain; charset=" + std::string(109, 'a')));
  EXPECT_FALSE(IsCorsSafelistedHeader(
      "cONTent-tYPE", "text/plain; charset=" + std::string(109, 'a')));
}

TEST_F(CorsTest, CheckCorsClientHintsSafelist) {
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", ""));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "abc"));
  EXPECT_TRUE(IsCorsSafelistedHeader("device-memory", "1.25"));
  EXPECT_TRUE(IsCorsSafelistedHeader("DEVICE-memory", "1.25"));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "1.25-2.5"));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "-1.25"));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "1e2"));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "inf"));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "-2.3"));
  EXPECT_FALSE(IsCorsSafelistedHeader("device-memory", "NaN"));
  EXPECT_FALSE(IsCorsSafelistedHeader("DEVICE-memory", "1.25.3"));
  EXPECT_FALSE(IsCorsSafelistedHeader("DEVICE-memory", "1."));
  EXPECT_FALSE(IsCorsSafelistedHeader("DEVICE-memory", ".1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("DEVICE-memory", "."));
  EXPECT_TRUE(IsCorsSafelistedHeader("DEVICE-memory", "1"));

  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", ""));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "abc"));
  EXPECT_TRUE(IsCorsSafelistedHeader("dpr", "1.25"));
  EXPECT_TRUE(IsCorsSafelistedHeader("Dpr", "1.25"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "1.25-2.5"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "-1.25"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "1e2"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "inf"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "-2.3"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "NaN"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "1.25.3"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "1."));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", ".1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("dpr", "."));
  EXPECT_TRUE(IsCorsSafelistedHeader("dpr", "1"));

  EXPECT_FALSE(IsCorsSafelistedHeader("width", ""));
  EXPECT_FALSE(IsCorsSafelistedHeader("width", "abc"));
  EXPECT_TRUE(IsCorsSafelistedHeader("width", "125"));
  EXPECT_TRUE(IsCorsSafelistedHeader("width", "1"));
  EXPECT_TRUE(IsCorsSafelistedHeader("WIDTH", "125"));
  EXPECT_FALSE(IsCorsSafelistedHeader("width", "125.2"));
  EXPECT_FALSE(IsCorsSafelistedHeader("width", "-125"));
  EXPECT_TRUE(IsCorsSafelistedHeader("width", "2147483648"));

  EXPECT_FALSE(IsCorsSafelistedHeader("viewport-width", ""));
  EXPECT_FALSE(IsCorsSafelistedHeader("viewport-width", "abc"));
  EXPECT_TRUE(IsCorsSafelistedHeader("viewport-width", "125"));
  EXPECT_TRUE(IsCorsSafelistedHeader("viewport-width", "1"));
  EXPECT_TRUE(IsCorsSafelistedHeader("viewport-Width", "125"));
  EXPECT_FALSE(IsCorsSafelistedHeader("viewport-width", "125.2"));
  EXPECT_TRUE(IsCorsSafelistedHeader("viewport-width", "2147483648"));
}

TEST_F(CorsTest, CorsUnsafeRequestHeaderNames) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(CorsUnsafeRequestHeaderNames({}), List({}));

  // Some headers are safelisted.
  EXPECT_EQ(CorsUnsafeRequestHeaderNames({{"content-type", "text/plain"},
                                          {"dpr", "12345"},
                                          {"aCCept", "en,ja"},
                                          {"accept-charset", "utf-8"},
                                          {"uSer-Agent", "foo"},
                                          {"hogE", "fuga"}}),
            List({"accept-charset", "user-agent", "hoge"}));

  // All headers are not safelisted.
  EXPECT_EQ(
      CorsUnsafeRequestHeaderNames({{"content-type", "text/html"},
                                    {"dpr", "123-45"},
                                    {"aCCept", "en,ja"},
                                    {"accept-charset", "utf-8"},
                                    {"uSer-Agent", "foo"},
                                    {"hogE", "fuga"}}),
      List({"content-type", "dpr", "accept-charset", "user-agent", "hoge"}));

  // |safelistValueSize| is 1024.
  EXPECT_EQ(
      CorsUnsafeRequestHeaderNames(
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
      CorsUnsafeRequestHeaderNames(
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
      CorsUnsafeRequestHeaderNames(
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

TEST_F(CorsTest, CorsUnsafeNotForbiddenRequestHeaderNames) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({}, false /* is_revalidating */),
      List({}));

  // "user-agent" is NOT forbidden per spec, but forbidden in Chromium.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"content-type", "text/plain"},
                                                {"dpr", "12345"},
                                                {"aCCept", "en,ja"},
                                                {"accept-charset", "utf-8"},
                                                {"uSer-Agent", "foo"},
                                                {"hogE", "fuga"}},
                                               false /* is_revalidating */),
      List({"hoge"}));

  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"content-type", "text/html"},
                                                {"dpr", "123-45"},
                                                {"aCCept", "en,ja"},
                                                {"accept-charset", "utf-8"},
                                                {"hogE", "fuga"}},
                                               false /* is_revalidating */),
      List({"content-type", "dpr", "hoge"}));

  // |safelistValueSize| is 1024.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames(
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
      CorsUnsafeNotForbiddenRequestHeaderNames(
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
      CorsUnsafeNotForbiddenRequestHeaderNames(
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

TEST_F(CorsTest, CorsUnsafeNotForbiddenRequestHeaderNamesWithRevalidating) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({}, true /* is_revalidating */),
      List({}));

  // These three headers will be ignored.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"If-MODifIED-since", "x"},
                                                {"iF-nONE-MATCh", "y"},
                                                {"CACHE-ContrOl", "z"}},
                                               true /* is_revalidating */),
      List({}));

  // Without is_revalidating set, these three headers will not be safelisted.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"If-MODifIED-since", "x"},
                                                {"iF-nONE-MATCh", "y"},
                                                {"CACHE-ContrOl", "z"}},
                                               false /* is_revalidating */),
      List({"if-modified-since", "if-none-match", "cache-control"}));
}

TEST_F(CorsTest, NoCorsSafelistedHeaderName) {
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("accept"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("AcCePT"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("accept-language"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("acCEPt-lAnguage"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("content-language"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("coNTENt-lAnguage"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("content-type"));
  EXPECT_TRUE(IsNoCorsSafelistedHeaderName("CONTENT-TYPE"));

  EXPECT_FALSE(IsNoCorsSafelistedHeaderName("range"));
  EXPECT_FALSE(IsNoCorsSafelistedHeaderName("cookie"));
  EXPECT_FALSE(IsNoCorsSafelistedHeaderName("foobar"));
}

TEST_F(CorsTest, PrivilegedNoCorsHeaderName) {
  EXPECT_TRUE(IsPrivilegedNoCorsHeaderName("range"));
  EXPECT_TRUE(IsPrivilegedNoCorsHeaderName("RanGe"));
  EXPECT_FALSE(IsPrivilegedNoCorsHeaderName("content-type"));
  EXPECT_FALSE(IsPrivilegedNoCorsHeaderName("foobar"));
  EXPECT_FALSE(IsPrivilegedNoCorsHeaderName("cookie"));
}

}  // namespace
}  // namespace cors
}  // namespace network
