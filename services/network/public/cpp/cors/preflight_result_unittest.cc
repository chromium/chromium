// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_result.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace cors {

namespace {

using PreflightResultTest = ::testing::Test;

struct TestCase {
  const std::string allow_methods;
  const std::string allow_headers;
  const mojom::CredentialsMode cache_credentials_mode;

  const std::string request_method;
  const std::string request_headers;
  const mojom::CredentialsMode request_credentials_mode;

  const base::Optional<CorsErrorStatus> expected_result;
};

const TestCase method_cases[] = {
    // Found in the preflight response.
    {"OPTIONS", "", mojom::CredentialsMode::kOmit, "OPTIONS", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"GET", "", mojom::CredentialsMode::kOmit, "GET", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"HEAD", "", mojom::CredentialsMode::kOmit, "HEAD", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"POST", "", mojom::CredentialsMode::kOmit, "POST", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"PUT", "", mojom::CredentialsMode::kOmit, "PUT", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"DELETE", "", mojom::CredentialsMode::kOmit, "DELETE", "",
     mojom::CredentialsMode::kOmit, base::nullopt},

    // Found in the safe list.
    {"", "", mojom::CredentialsMode::kOmit, "GET", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"", "", mojom::CredentialsMode::kOmit, "HEAD", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"", "", mojom::CredentialsMode::kOmit, "POST", "",
     mojom::CredentialsMode::kOmit, base::nullopt},

    // By '*'.
    {"*", "", mojom::CredentialsMode::kOmit, "OPTIONS", "",
     mojom::CredentialsMode::kOmit, base::nullopt},

    // Cache allowing multiple methods.
    {"GET, PUT, DELETE", "", mojom::CredentialsMode::kOmit, "GET", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"GET, PUT, DELETE", "", mojom::CredentialsMode::kOmit, "PUT", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"GET, PUT, DELETE", "", mojom::CredentialsMode::kOmit, "DELETE", "",
     mojom::CredentialsMode::kOmit, base::nullopt},

    // Not found in the preflight response and the safe lit.
    {"", "", mojom::CredentialsMode::kOmit, "OPTIONS", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "OPTIONS")},
    {"", "", mojom::CredentialsMode::kOmit, "PUT", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"", "", mojom::CredentialsMode::kOmit, "DELETE", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "DELETE")},
    {"GET", "", mojom::CredentialsMode::kOmit, "PUT", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"GET, POST, DELETE", "", mojom::CredentialsMode::kOmit, "PUT", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},

    // Request method is normalized to upper-case, but allowed methods is not.
    // Comparison is in case-sensitive, that means allowed methods should be in
    // upper case.
    {"put", "", mojom::CredentialsMode::kOmit, "PUT", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"put", "", mojom::CredentialsMode::kOmit, "put", "",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                     "put")},
    {"PUT", "", mojom::CredentialsMode::kOmit, "put", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
    // ... But, GET is always allowed by the safe list.
    {"get", "", mojom::CredentialsMode::kOmit, "GET", "",
     mojom::CredentialsMode::kOmit, base::nullopt},
};

const TestCase header_cases[] = {
    // Found in the preflight response.
    {"GET", "X-MY-HEADER", mojom::CredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t", mojom::CredentialsMode::kOmit, base::nullopt},
    {"GET", "X-MY-HEADER, Y-MY-HEADER", mojom::CredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t\r\nY-MY-HEADER:t", mojom::CredentialsMode::kOmit,
     base::nullopt},
    {"GET", "x-my-header, Y-MY-HEADER", mojom::CredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t\r\ny-my-header:t", mojom::CredentialsMode::kOmit,
     base::nullopt},

    // Found in the safe list.
    {"GET", "", mojom::CredentialsMode::kOmit, "GET", "Accept:*/*",
     mojom::CredentialsMode::kOmit, base::nullopt},

    // By '*'.
    {"GET", "*", mojom::CredentialsMode::kOmit, "GET", "xyzzy:t",
     mojom::CredentialsMode::kOmit, base::nullopt},
    {"GET", "*", mojom::CredentialsMode::kInclude, "GET", "xyzzy:t",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "xyzzy")},

    // Forbidden headers can pass.
    {"GET", "", mojom::CredentialsMode::kOmit, "GET", "Host: www.google.com",
     mojom::CredentialsMode::kOmit, base::nullopt},

    // Not found in the preflight response and the safe list.
    {"GET", "", mojom::CredentialsMode::kOmit, "GET", "X-MY-HEADER:t",
     mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "x-my-header")},
    {"GET", "X-SOME-OTHER-HEADER", mojom::CredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t", mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "x-my-header")},
    {"GET", "X-MY-HEADER", mojom::CredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t\r\nY-MY-HEADER:t", mojom::CredentialsMode::kOmit,
     CorsErrorStatus(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
                     "y-my-header")},
};

TEST_F(PreflightResultTest, MaxAge) {
  std::unique_ptr<base::SimpleTestTickClock> tick_clock =
      std::make_unique<base::SimpleTestTickClock>();
  PreflightResult::SetTickClockForTesting(tick_clock.get());

  std::unique_ptr<PreflightResult> result1 =
      PreflightResult::Create(mojom::CredentialsMode::kOmit, base::nullopt,
                              base::nullopt, std::string("573"), nullptr);
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(573),
            result1->absolute_expiry_time());

  // Negative values are invalid. The preflight result itself can be usable, but
  // should not cache such results. PreflightResult expresses it as a result
  // with 'Access-Control-Max-Age: 0'.
  std::unique_ptr<PreflightResult> result2 =
      PreflightResult::Create(mojom::CredentialsMode::kOmit, base::nullopt,
                              base::nullopt, std::string("-765"), nullptr);
  EXPECT_EQ(base::TimeTicks(), result2->absolute_expiry_time());

  PreflightResult::SetTickClockForTesting(nullptr);
}

TEST_F(PreflightResultTest, EnsureMethods) {
  for (const auto& test : method_cases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, base::nullopt, nullptr);
    ASSERT_TRUE(result);
    EXPECT_EQ(test.expected_result,
              result->EnsureAllowedCrossOriginMethod(test.request_method));
  }
}

TEST_F(PreflightResultTest, EnsureHeaders) {
  for (const auto& test : header_cases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, base::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(test.request_headers);
    EXPECT_EQ(test.expected_result,
              result->EnsureAllowedCrossOriginHeaders(headers, false));
  }
}

TEST_F(PreflightResultTest, EnsureRequest) {
  for (const auto& test : method_cases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, base::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    if (!test.request_headers.empty())
      headers.AddHeadersFromString(test.request_headers);
    EXPECT_EQ(
        test.expected_result == base::nullopt,
        result->EnsureAllowedRequest(test.request_credentials_mode,
                                     test.request_method, headers, false));
  }

  for (const auto& test : header_cases) {
    std::unique_ptr<PreflightResult> result =
        PreflightResult::Create(test.cache_credentials_mode, test.allow_methods,
                                test.allow_headers, base::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    if (!test.request_headers.empty())
      headers.AddHeadersFromString(test.request_headers);
    EXPECT_EQ(
        test.expected_result == base::nullopt,
        result->EnsureAllowedRequest(test.request_credentials_mode,
                                     test.request_method, headers, false));
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
                                base::nullopt, base::nullopt, nullptr);
    ASSERT_TRUE(result);
    net::HttpRequestHeaders headers;
    EXPECT_EQ(test.expected_result,
              result->EnsureAllowedRequest(test.request_credentials_mode, "GET",
                                           headers, false));
  }
}

}  // namespace

}  // namespace cors

}  // namespace network
