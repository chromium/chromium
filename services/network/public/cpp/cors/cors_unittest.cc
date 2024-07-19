// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/public/cpp/cors/cors.h"

#include <limits.h>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {
namespace {

using CorsTest = testing::Test;

// Tests if CheckAccess detects kWildcardOriginNotAllowed error correctly.
TEST_F(CorsTest, CheckAccessDetectsWildcardOriginNotAllowed) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));
  const std::string allow_all_header("*");

  // Access-Control-Allow-Origin '*' works.
  const auto result =
      CheckAccess(response_url, allow_all_header /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  EXPECT_TRUE(result.has_value());

  // Access-Control-Allow-Origin '*' should not be allowed if credentials mode
  // is kInclude.
  const auto result2 =
      CheckAccess(response_url, allow_all_header /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kInclude, origin);
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(mojom::CorsError::kWildcardOriginNotAllowed,
            result2.error().cors_error);
}

// Tests if CheckAccess detects kMissingAllowOriginHeader error correctly.
TEST_F(CorsTest, CheckAccessDetectsMissingAllowOriginHeader) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));

  // Access-Control-Allow-Origin is missed.
  const auto result =
      CheckAccess(response_url, std::nullopt /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(mojom::CorsError::kMissingAllowOriginHeader,
            result.error().cors_error);
}

// Tests if CheckAccess detects kMultipleAllowOriginValues error
// correctly.
TEST_F(CorsTest, CheckAccessDetectsMultipleAllowOriginValues) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));

  const std::string space_separated_multiple_origins(
      "http://example.com http://another.example.com");
  const auto result1 = CheckAccess(
      response_url, space_separated_multiple_origins /* allow_origin_header */,
      std::nullopt /* allow_credentials_header */,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(mojom::CorsError::kMultipleAllowOriginValues,
            result1.error().cors_error);

  const std::string comma_separated_multiple_origins(
      "http://example.com,http://another.example.com");
  const auto result2 = CheckAccess(
      response_url, comma_separated_multiple_origins /* allow_origin_header */,
      std::nullopt /* allow_credentials_header */,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(mojom::CorsError::kMultipleAllowOriginValues,
            result2.error().cors_error);
}

// Tests if CheckAccess detects kInvalidAllowOriginValue error correctly.
TEST_F(CorsTest, CheckAccessDetectsInvalidAllowOriginValue) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));

  const auto result = CheckAccess(
      response_url, std::string("invalid.origin") /* allow_origin_header */,
      std::nullopt /* allow_credentials_header */,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(mojom::CorsError::kInvalidAllowOriginValue,
            result.error().cors_error);
  EXPECT_EQ("invalid.origin", result.error().failed_parameter);
}

// Tests if CheckAccess detects kAllowOriginMismatch error correctly.
TEST_F(CorsTest, CheckAccessDetectsAllowOriginMismatch) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));

  const auto result1 =
      CheckAccess(response_url, origin.Serialize() /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_TRUE(result1.has_value());

  const auto result2 = CheckAccess(
      response_url,
      std::string("http://not.google.com") /* allow_origin_header */,
      std::nullopt /* allow_credentials_header */,
      network::mojom::CredentialsMode::kOmit, origin);
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(mojom::CorsError::kAllowOriginMismatch, result2.error().cors_error);
  EXPECT_EQ("http://not.google.com", result2.error().failed_parameter);

  // Allow "null" value to match serialized unique origins.
  const std::string null_string("null");
  const url::Origin null_origin;
  EXPECT_EQ(null_string, null_origin.Serialize());

  const auto result3 =
      CheckAccess(response_url, null_string /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, null_origin);
  EXPECT_TRUE(result3.has_value());
}

// Tests if CheckAccess detects kInvalidAllowCredentials error correctly.
TEST_F(CorsTest, CheckAccessDetectsInvalidAllowCredential) {
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));

  const auto result1 =
      CheckAccess(response_url, origin.Serialize() /* allow_origin_header */,
                  std::string("true") /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kInclude, origin);
  ASSERT_TRUE(result1.has_value());

  const auto restul2 =
      CheckAccess(response_url, origin.Serialize() /* allow_origin_header */,
                  std::string("fuga") /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kInclude, origin);
  ASSERT_FALSE(restul2.has_value());
  EXPECT_EQ(mojom::CorsError::kInvalidAllowCredentials,
            restul2.error().cors_error);
  EXPECT_EQ("fuga", restul2.error().failed_parameter);
}

// Should match unexposed enum in cors.cc
enum class AccessCheckResult {
  kPermitted = 0,
  kNotPermitted = 1,
  kPermittedInPreflight = 2,
  kNotPermittedInPreflight = 3,

  kMaxValue = kNotPermittedInPreflight,
};
constexpr char kAccessCheckHistogram[] = "Net.Cors.AccessCheckResult";
constexpr char kAccessCheckHistogramNotSecure[] =
    "Net.Cors.AccessCheckResult.NotSecureRequestor";

TEST_F(CorsTest, CheckAccessAndReportMetricsForPermittedSecureOrigin) {
  base::HistogramTester histogram_tester;
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("https://google.com"));

  EXPECT_TRUE(CheckAccessAndReportMetrics(
                  response_url, origin.Serialize() /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin)
                  .has_value());
  histogram_tester.ExpectUniqueSample(kAccessCheckHistogram,
                                      AccessCheckResult::kPermitted, 1);
  histogram_tester.ExpectTotalCount(kAccessCheckHistogramNotSecure, 0);
}

TEST_F(CorsTest, CheckAccessAndReportMetricsForPermittedNotSecureOrigin) {
  base::HistogramTester histogram_tester;
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("http://google.com"));

  EXPECT_TRUE(CheckAccessAndReportMetrics(
                  response_url, origin.Serialize() /* allow_origin_header */,
                  std::nullopt /* allow_credentials_header */,
                  network::mojom::CredentialsMode::kOmit, origin)
                  .has_value());
  histogram_tester.ExpectUniqueSample(kAccessCheckHistogram,
                                      AccessCheckResult::kPermitted, 1);
  histogram_tester.ExpectUniqueSample(kAccessCheckHistogramNotSecure,
                                      AccessCheckResult::kPermitted, 1);
}

TEST_F(CorsTest, CheckAccessAndReportMetricsForNotPermittedSecureOrigin) {
  base::HistogramTester histogram_tester;
  const GURL response_url("http://example.com/data");
  const url::Origin origin = url::Origin::Create(GURL("https://google.com"));

  EXPECT_FALSE(CheckAccessAndReportMetrics(
                   response_url, std::nullopt /* allow_origin_header */,
                   std::nullopt /* allow_credentials_header */,
                   network::mojom::CredentialsMode::kOmit, origin)
                   .has_value());

  histogram_tester.ExpectUniqueSample(kAccessCheckHistogram,
                                      AccessCheckResult::kNotPermitted, 1);
  histogram_tester.ExpectTotalCount(kAccessCheckHistogramNotSecure, 0);
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
    auto* end = kAllowed + std::size(kAllowed) - 1;
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
    auto* end = kAllowed + std::size(kAllowed) - 1;
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

TEST_F(CorsTest, SafelistedSecCHPrefersColorScheme) {
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-Prefers-Color-Scheme",
                                     "\"Prefers-Color-Scheme!\""));
}

TEST_F(CorsTest, SafelistedSecCHPrefersReducedMotion) {
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-Prefers-Reduced-Motion",
                                     "\"Prefers-Reduced-Motion!\""));
}

TEST_F(CorsTest, SafelistedSecCHPrefersReducedTransparency) {
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-Prefers-Reduced-Transparency",
                                     "\"Prefers-Reduced-Transparency!\""));
}

TEST_F(CorsTest, SafelistedSecCHUAFormFactors) {
  EXPECT_TRUE(
      IsCorsSafelistedHeader("Sec-CH-UA-Form-Factors", "\"Form Factors!\""));
}

TEST_F(CorsTest, SafelistedSecCHUA) {
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA", "\"User Agent!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Platform", "\"Platform!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Platform-Version",
                                     "\"Platform-Version!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Arch", "\"Architecture!\""));
  EXPECT_TRUE(IsCorsSafelistedHeader("Sec-CH-UA-Model", "\"Model!\""));

  // TODO(mkwst): Validate that `Sec-CH-UA-*` is a structured header.
  // https://crbug.com/924969
}

TEST_F(CorsTest, SafelistedSecCHViewportHeight) {
  EXPECT_TRUE(
      IsCorsSafelistedHeader("Sec-CH-Viewport-Height", "\"Viewport-Height!\""));
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
    auto* end = kAllowed + std::size(kAllowed) - 1;
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
    const auto* const end = kAllowed + std::size(kAllowed) - 1;
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

TEST_F(CorsTest, CheckCorsClientHintsNetworkQuality) {
  EXPECT_FALSE(IsCorsSafelistedHeader("rtt", ""));
  EXPECT_TRUE(IsCorsSafelistedHeader("rtt", "1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("rtt", "-1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("rtt", "1.0"));
  EXPECT_FALSE(IsCorsSafelistedHeader("rtt", "-1.0"));
  EXPECT_FALSE(IsCorsSafelistedHeader("rtt", "2g"));
  EXPECT_FALSE(IsCorsSafelistedHeader("rtt", "6g"));

  EXPECT_FALSE(IsCorsSafelistedHeader("downlink", ""));
  EXPECT_TRUE(IsCorsSafelistedHeader("downlink", "1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("downlink", "-1"));
  EXPECT_TRUE(IsCorsSafelistedHeader("downlink", "1.0"));
  EXPECT_FALSE(IsCorsSafelistedHeader("downlink", "-1.0"));
  EXPECT_FALSE(IsCorsSafelistedHeader("downlink", "foo"));
  EXPECT_FALSE(IsCorsSafelistedHeader("downlink", "2g"));
  EXPECT_FALSE(IsCorsSafelistedHeader("downlink", "6g"));

  EXPECT_FALSE(IsCorsSafelistedHeader("ect", ""));
  EXPECT_FALSE(IsCorsSafelistedHeader("ect", "1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("ect", "-1"));
  EXPECT_FALSE(IsCorsSafelistedHeader("ect", "1.0"));
  EXPECT_FALSE(IsCorsSafelistedHeader("ect", "-1.0"));
  EXPECT_FALSE(IsCorsSafelistedHeader("ect", "foo"));
  EXPECT_TRUE(IsCorsSafelistedHeader("ect", "2g"));
  EXPECT_FALSE(IsCorsSafelistedHeader("ect", "6g"));
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

TEST_F(CorsTest, CheckCorsRangeSafelist) {
  // Missing values
  EXPECT_FALSE(IsCorsSafelistedHeader("range", ""));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "500"));

  // Case
  EXPECT_TRUE(IsCorsSafelistedHeader("range", "bytes=100-200"));
  EXPECT_TRUE(IsCorsSafelistedHeader("Range", "bytes=100-200"));
  EXPECT_TRUE(IsCorsSafelistedHeader("RANGE", "bytes=100-200"));
  EXPECT_TRUE(IsCorsSafelistedHeader("range", "BYTES=100-200"));

  // Valid values
  EXPECT_TRUE(IsCorsSafelistedHeader("range", "bytes=100-"));

  // Multiple ranges
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=100-200,300-400"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=100-200,400"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=100-200-400"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=100-200,400-"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=-50,100-"));

  // Invalid ranges
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=200-100"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=-200--100"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=-50-50"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=-200"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=100"));

  // Invalid charset.
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes = 100-200"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes =100-200"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", "bytes=,100-200"));
  EXPECT_FALSE(IsCorsSafelistedHeader("range", ",bytes=,100-200"));
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

TEST_F(CorsTest, IsForbiddenMethod) {
  EXPECT_TRUE(IsForbiddenMethod("connect"));
  EXPECT_TRUE(IsForbiddenMethod("CONNECT"));
  EXPECT_TRUE(IsForbiddenMethod("Connect"));
  EXPECT_TRUE(IsForbiddenMethod("CoNnEcT"));
  EXPECT_FALSE(IsForbiddenMethod("C0NNECT"));

  EXPECT_TRUE(IsForbiddenMethod("trace"));
  EXPECT_TRUE(IsForbiddenMethod("track"));
  EXPECT_FALSE(IsForbiddenMethod("trac"));
  EXPECT_FALSE(IsForbiddenMethod("tracz"));
}

}  // namespace
}  // namespace network::cors
