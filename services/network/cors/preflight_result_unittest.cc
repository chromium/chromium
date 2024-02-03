// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_result.h"

#include "base/check_deref.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::cors {

namespace {

using PreflightResultTest = ::testing::Test;

constexpr std::optional<mojom::CorsError> kNoError;

struct TestCase {
  const std::string allow_methods;
  const std::string allow_headers;
  const mojom::CredentialsMode cache_credentials_mode;

  const std::string request_method;
  const std::vector<std::pair<std::string, std::string>> request_headers;
  const mojom::CredentialsMode request_credentials_mode;

  const std::optional<CorsErrorStatus> expected_result;
};

const TestCase kMethodCases[] = {
    // Found in the preflight response.
    {"OPTIONS",
     "",
     mojom::CredentialsMode::kOmit,
     "OPTIONS",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"GET",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"HEAD",
     "",
     mojom::CredentialsMode::kOmit,
     "HEAD",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"POST",
     "",
     mojom::CredentialsMode::kOmit,
     "POST",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"PUT",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"DELETE",
     "",
     mojom::CredentialsMode::kOmit,
     "DELETE",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    // Access-Control-Allow-Methods = #method, method = token.
    // So a non-standard method is accepted as well.
    {"FOOBAR",
     "",
     mojom::CredentialsMode::kOmit,
     "FOOBAR",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // Found in the safe list.
    {"",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"",
     "",
     mojom::CredentialsMode::kOmit,
     "HEAD",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"",
     "",
     mojom::CredentialsMode::kOmit,
     "POST",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // By '*'.
    {"*",
     "",
     mojom::CredentialsMode::kOmit,
     "OPTIONS",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // Cache allowing multiple methods.
    {"GET, PUT, DELETE",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"GET, PUT, DELETE",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"GET, PUT, DELETE",
     "",
     mojom::CredentialsMode::kOmit,
     "DELETE",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // Not found in the preflight response or the safe list.
    {"",
     "",
     mojom::CredentialsMode::kOmit,
     "OPTIONS",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "OPTIONS")},
    {"",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"",
     "",
     mojom::CredentialsMode::kOmit,
     "DELETE",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "DELETE")},
    {"GET",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"GET, POST, DELETE",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},

    // Empty entries in the allow_methods list are ignored.
    {"GET,,PUT",
     "",
     mojom::CredentialsMode::kOmit,
     "",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "")},
    {"GET, ,PUT",
     "",
     mojom::CredentialsMode::kOmit,
     " ",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     " ")},
    // A valid list can contain empty entries so the remaining non-empty
    // entries are accepted.
    {"GET, ,PUT",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // Neither request methods nor allowed methods are normalized to upper-case,
    // no matter whether the method is listed in
    // https://fetch.spec.whatwg.org/#concept-method-normalize,
    // because request methods should be normalized when requests are created
    // (e.g. https://fetch.spec.whatwg.org/#dom-request), before entering the
    // network service.
    // Comparison is in case-sensitive, that means allowed methods should be in
    // upper case.
    {"put",
     "",
     mojom::CredentialsMode::kOmit,
     "PUT",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"PUT",
     "",
     mojom::CredentialsMode::kOmit,
     "put",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "put")},
    {"put",
     "",
     mojom::CredentialsMode::kOmit,
     "put",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"patch",
     "",
     mojom::CredentialsMode::kOmit,
     "PATCH",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PATCH")},
    {"PATCH",
     "",
     mojom::CredentialsMode::kOmit,
     "patch",
     {},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "patch")},
    {"patch",
     "",
     mojom::CredentialsMode::kOmit,
     "patch",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // ... But, GET is always allowed by the safe list.
    {"get",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
};

const TestCase kHeaderCases[] = {
    // Found in the preflight response.
    {"GET",
     "X-MY-HEADER",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"X-MY-HEADER", "t"}},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"GET",
     "X-MY-HEADER, Y-MY-HEADER",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"X-MY-HEADER", "t"}, {"Y-MY-HEADER", "t"}},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"GET",
     "x-my-header, Y-MY-HEADER",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"X-MY-HEADER", "t"}, {"y-my-header", "t"}},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // Found in the safe list.
    {"GET",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"Accept", "*/*"}},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // By '*'.
    {"GET",
     "*",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"xyzzy", "t"}},
     mojom::CredentialsMode::kOmit,
     std::nullopt},
    {"GET",
     "*",
     mojom::CredentialsMode::kInclude,
     "GET",
     {{"xyzzy", "t"}},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "xyzzy")},

    // Forbidden headers can pass.
    {"GET",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"Host", "www.google.com"}},
     mojom::CredentialsMode::kOmit,
     std::nullopt},

    // Not found in the preflight response and the safe list.
    {"GET",
     "",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"X-MY-HEADER", "t"}},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "x-my-header")},
    {"GET",
     "X-SOME-OTHER-HEADER",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"X-MY-HEADER", "t"}},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "x-my-header")},
    {"GET",
     "X-MY-HEADER",
     mojom::CredentialsMode::kOmit,
     "GET",
     {{"X-MY-HEADER", "t"}, {"Y-MY-HEADER", "t"}},
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "y-my-header")},
};

TEST_F(PreflightResultTest, MaxAge) {
  std::unique_ptr<base::SimpleTestTickClock> tick_clock =
      std::make_unique<base::SimpleTestTickClock>();
  PreflightResult::SetTickClockForTesting(tick_clock.get());

  std::unique_ptr<PreflightResult> result1 =
      PreflightResult::Create(mojom::CredentialsMode::kOmit, std::nullopt,
                              std::nullopt, std::string("573"), nullptr);
  EXPECT_EQ(base::TimeTicks() + base::Seconds(573),
            result1->absolute_expiry_time());

  std::unique_ptr<PreflightResult> result2 =
      PreflightResult::Create(mojom::CredentialsMode::kOmit, std::nullopt,
                              std::nullopt, std::string("-765"), nullptr);
  EXPECT_EQ(base::TimeTicks(), result2->absolute_expiry_time());

  PreflightResult::SetTickClockForTesting(nullptr);
}

TEST_F(PreflightResultTest, EnsureMethods) {
  for (const auto& test : kMethodCases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, std::nullopt, nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(test.expected_result, result->EnsureAllowedCrossOriginMethod(
                                        test.request_method, true));
  }
}

TEST_F(PreflightResultTest, EnsureHeaders) {
  for (const auto& test : kHeaderCases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, std::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    for (const auto& header : test.request_headers)
      headers.SetHeader(header.first, header.second);
    EXPECT_EQ(test.expected_result,
              result->EnsureAllowedCrossOriginHeaders(
                  headers, false, NonWildcardRequestHeadersSupport(false)));
  }
}

TEST_F(PreflightResultTest, EnsureRequest) {
  for (const auto& test : kMethodCases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, std::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    for (const auto& header : test.request_headers)
      headers.SetHeader(header.first, header.second);
    EXPECT_EQ(test.expected_result == std::nullopt,
              result->EnsureAllowedRequest(
                  test.request_credentials_mode, test.request_method, headers,
                  false, NonWildcardRequestHeadersSupport(false), true));
  }

  for (const auto& test : kHeaderCases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, std::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    for (const auto& header : test.request_headers)
      headers.SetHeader(header.first, header.second);
    EXPECT_EQ(test.expected_result == std::nullopt,
              result->EnsureAllowedRequest(
                  test.request_credentials_mode, test.request_method, headers,
                  false, NonWildcardRequestHeadersSupport(false), true));
  }

  struct {
    const mojom::CredentialsMode cache_credentials_mode;
    const mojom::CredentialsMode request_credentials_mode;
    const bool expected_result;
  } credentials_cases[] = {
      // Different credential modes.
      {mojom::CredentialsMode::kInclude, mojom::CredentialsMode::kOmit, true},
      {mojom::CredentialsMode::kInclude, mojom::CredentialsMode::kInclude,
       true},

      // Credential mode mismatch.
      {mojom::CredentialsMode::kOmit, mojom::CredentialsMode::kOmit, true},
      {mojom::CredentialsMode::kOmit, mojom::CredentialsMode::kInclude, false},
  };

  for (const auto& test : credentials_cases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, std::string("GET"),
                                std::nullopt, std::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    EXPECT_EQ(test.expected_result,
              result->EnsureAllowedRequest(
                  test.request_credentials_mode, "GET", headers, false,
                  NonWildcardRequestHeadersSupport(false), true));
  }
}

struct ParseHeaderListTestCase {
  const std::string input;
  const std::vector<std::pair<std::string, std::string>> values_to_be_accepted;
  const std::optional<mojom::CorsError> strict_check_result;
};

const ParseHeaderListTestCase kParseHeadersCases[] = {
    {"bad value", {}, mojom::CorsError::kInvalidAllowHeadersPreflightResponse},
    {"X-MY-HEADER, ", {{"X-MY-HEADER", "t"}}, kNoError},
    {"", {}, kNoError},
    {", X-MY-HEADER, Y-MY-HEADER, ,",
     {{"X-MY-HEADER", "t"}, {"Y-MY-HEADER", "t"}},
     kNoError}};

TEST_F(PreflightResultTest, ParseAllowControlAllowHeaders) {
  for (const auto& test : kParseHeadersCases) {
    std::optional<mojom::CorsError> error;
    std::unique_ptr<PreflightResult> result = PreflightResult::Create(
        mojom::CredentialsMode::kOmit, /*allow_methods_header=*/std::nullopt,
        test.input, /*max_age_header=*/std::nullopt, &error);
    EXPECT_EQ(error, test.strict_check_result);

    if (test.strict_check_result == kNoError) {
      for (const auto& request_header : test.values_to_be_accepted) {
        net::HttpRequestHeaders headers;
        headers.SetHeader(request_header.first, request_header.second);
        EXPECT_EQ(std::nullopt,
                  result->EnsureAllowedCrossOriginHeaders(
                      headers, false, NonWildcardRequestHeadersSupport(false)));
      }
    }
  }
}

struct ParseMethodListTestCase {
  const std::string input;
  const std::vector<std::string> values_to_be_accepted;
  const std::optional<mojom::CorsError> strict_check_result;
};

const ParseMethodListTestCase kParseMethodsCases[] = {
    {"bad value", {}, mojom::CorsError::kInvalidAllowMethodsPreflightResponse},
    {"GET, ", {"GET"}, kNoError},
    {"", {}, kNoError},
    {", GET, POST, ,", {"GET", "POST"}, kNoError}};

TEST_F(PreflightResultTest, ParseAllowControlAllowMethods) {
  for (const auto& test : kParseMethodsCases) {
    std::optional<mojom::CorsError> error;
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(mojom::CredentialsMode::kOmit, test.input,
                                /*allow_headers_header=*/std::nullopt,
                                /*max_age_header=*/std::nullopt, &error);
    EXPECT_EQ(error, test.strict_check_result);

    if (test.strict_check_result == kNoError) {
      for (const auto& request_method : test.values_to_be_accepted) {
        EXPECT_EQ(std::nullopt,
                  result->EnsureAllowedCrossOriginMethod(request_method, true));
      }
    }
  }
}

net::HttpRequestHeaders CreateHeaders(
    const std::vector<std::pair<std::string, std::string>>& data) {
  net::HttpRequestHeaders headers;
  for (const auto& pair : data) {
    headers.SetHeader(pair.first, pair.second);
  }
  return headers;
}

TEST_F(PreflightResultTest,
       ParseAuthorizationWithoutNonWildcardRequestHeadersSupport) {
  constexpr auto kOmit = mojom::CredentialsMode::kOmit;
  const std::optional<std::string> kMethods = "GET";
  const std::optional<std::string> kMaxAge = std::nullopt;

  std::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(kOmit, kMethods, "*", kMaxAge, &error);
  ASSERT_EQ(error, std::nullopt);
  net::HttpRequestHeaders headers = CreateHeaders({{"auThorization", "x"}});
  const auto status = result->EnsureAllowedCrossOriginHeaders(
      headers, false, NonWildcardRequestHeadersSupport(false));
  EXPECT_EQ(status, std::nullopt);
}

TEST_F(PreflightResultTest,
       ParseAuthorizationWithNonWildcardRequestHeadersSupport) {
  constexpr auto kOmit = mojom::CredentialsMode::kOmit;
  const std::optional<std::string> kMethods = "GET";
  const std::optional<std::string> kMaxAge = std::nullopt;

  std::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(kOmit, kMethods, "*", kMaxAge, &error);
  ASSERT_EQ(error, std::nullopt);
  net::HttpRequestHeaders headers = CreateHeaders({{"auThorization", "x"}});
  const auto status = result->EnsureAllowedCrossOriginHeaders(
      headers, false, NonWildcardRequestHeadersSupport(true));
  ASSERT_NE(status, std::nullopt);
  EXPECT_EQ(status->cors_error,
            mojom::CorsError::kHeaderDisallowedByPreflightResponse);
  EXPECT_TRUE(status->has_authorization_covered_by_wildcard_on_preflight);
}

TEST_F(
    PreflightResultTest,
    ParseAuthorizationWithNonWildcardRequestHeadersSupportAndAuthorizationOnPreflightResponse) {
  constexpr auto kOmit = mojom::CredentialsMode::kOmit;
  const std::optional<std::string> kMethods = "GET";
  const std::optional<std::string> kMaxAge = std::nullopt;

  std::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(kOmit, kMethods, "*, AUTHORIZAtion",
                                        kMaxAge, &error);
  ASSERT_EQ(error, std::nullopt);
  net::HttpRequestHeaders headers = CreateHeaders({{"auThorization", "x"}});
  const auto status = result->EnsureAllowedCrossOriginHeaders(
      headers, false, NonWildcardRequestHeadersSupport(true));
  EXPECT_EQ(status, std::nullopt);
}

TEST_F(PreflightResultTest, AuthorizationIsCoveredByAuthorization) {
  constexpr auto kOmit = mojom::CredentialsMode::kOmit;
  const std::optional<std::string> kMethods = "GET";
  const std::optional<std::string> kMaxAge = std::nullopt;

  std::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(kOmit, kMethods, "*, AUTHORIZAtion",
                                        kMaxAge, &error);
  ASSERT_EQ(error, std::nullopt);
  net::HttpRequestHeaders headers = CreateHeaders({{"auThorization", "x"}});
  EXPECT_FALSE(result->HasAuthorizationCoveredByWildcard(
      CreateHeaders({{"authoRization", "x"}, {"foo", "bar"}})));
}

TEST_F(PreflightResultTest, AuthorizationIsCoveredByWildCard) {
  constexpr auto kOmit = mojom::CredentialsMode::kOmit;
  const std::optional<std::string> kMethods = "GET";
  const std::optional<std::string> kMaxAge = std::nullopt;

  std::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(kOmit, kMethods, "*", kMaxAge, &error);
  ASSERT_EQ(error, std::nullopt);
  EXPECT_TRUE(result->HasAuthorizationCoveredByWildcard(
      CreateHeaders({{"authoRization", "x"}, {"foo", "bar"}})));
}

TEST_F(PreflightResultTest, NoAuthorization) {
  constexpr auto kOmit = mojom::CredentialsMode::kOmit;
  const std::optional<std::string> kMethods = "GET";
  const std::optional<std::string> kMaxAge = std::nullopt;

  std::optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(kOmit, kMethods, "*", kMaxAge, &error);
  ASSERT_EQ(error, std::nullopt);
  EXPECT_FALSE(result->HasAuthorizationCoveredByWildcard(
      CreateHeaders({{"foo", "bar"}})));
}

struct TestCaseForNetLogParams {
  const std::string allow_methods;
  const std::string allow_headers;

  const std::string expected_methods;
  const std::string expected_headers;
};

TEST_F(PreflightResultTest, NetLogParams) {
  const struct {
    const char* allow_methods;
    const char* allow_headers;

    const char* expected_methods;
    const char* expected_headers;
  } kNetLogParamsCases[] = {
      {"", "X-MY-HEADER", "", "x-my-header"},
      {"GET", "X-MY-HEADER", "GET", "x-my-header"},
      {"GET, POST", "X-MY-HEADER", "GET,POST", "x-my-header"},
      {"GET", "", "GET", ""},
      {"GET", "X-MY-HEADER", "GET", "x-my-header"},
      {"GET", "X-MY-HEADER, Y-MY-HEADER", "GET", "x-my-header,y-my-header"}};

  for (const auto& test : kNetLogParamsCases) {
    std::unique_ptr<PreflightResult> result = PreflightResult::Create(
        mojom::CredentialsMode::kOmit, test.allow_methods, test.allow_headers,
        std::nullopt, nullptr);
    ASSERT_TRUE(result);
    base::Value::Dict dict = result->NetLogParams();
    EXPECT_EQ(CHECK_DEREF(dict.FindString("access-control-allow-methods")),
              test.expected_methods);
    EXPECT_EQ(CHECK_DEREF(dict.FindString("access-control-allow-headers")),
              test.expected_headers);
  }
}

}  // namespace

}  // namespace network::cors
