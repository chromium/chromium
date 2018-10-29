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
  const mojom::FetchCredentialsMode cache_credentials_mode;

  const std::string request_method;
  const std::string request_headers;
  const mojom::FetchCredentialsMode request_credentials_mode;

  const base::Optional<CORSErrorStatus> expected_result;
};

const TestCase method_cases[] = {
    // Found in the preflight response.
    {"OPTIONS", "", mojom::FetchCredentialsMode::kOmit, "OPTIONS", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"GET", "", mojom::FetchCredentialsMode::kOmit, "GET", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"HEAD", "", mojom::FetchCredentialsMode::kOmit, "HEAD", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"POST", "", mojom::FetchCredentialsMode::kOmit, "POST", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"PUT", "", mojom::FetchCredentialsMode::kOmit, "PUT", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"DELETE", "", mojom::FetchCredentialsMode::kOmit, "DELETE", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // Found in the safe list.
    {"", "", mojom::FetchCredentialsMode::kOmit, "GET", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"", "", mojom::FetchCredentialsMode::kOmit, "HEAD", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"", "", mojom::FetchCredentialsMode::kOmit, "POST", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // By '*'.
    {"*", "", mojom::FetchCredentialsMode::kOmit, "OPTIONS", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // Cache allowing multiple methods.
    {"GET, PUT, DELETE", "", mojom::FetchCredentialsMode::kOmit, "GET", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"GET, PUT, DELETE", "", mojom::FetchCredentialsMode::kOmit, "PUT", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"GET, PUT, DELETE", "", mojom::FetchCredentialsMode::kOmit, "DELETE", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // Not found in the preflight response and the safe lit.
    {"", "", mojom::FetchCredentialsMode::kOmit, "OPTIONS", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "OPTIONS")},
    {"", "", mojom::FetchCredentialsMode::kOmit, "PUT", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"", "", mojom::FetchCredentialsMode::kOmit, "DELETE", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "DELETE")},
    {"GET", "", mojom::FetchCredentialsMode::kOmit, "PUT", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"GET, POST, DELETE", "", mojom::FetchCredentialsMode::kOmit, "PUT", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "PUT")},

    // Request method is normalized to upper-case, but allowed methods is not.
    // Comparison is in case-sensitive, that means allowed methods should be in
    // upper case.
    {"put", "", mojom::FetchCredentialsMode::kOmit, "PUT", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "PUT")},
    {"put", "", mojom::FetchCredentialsMode::kOmit, "put", "",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kMethodDisallowedByPreflightResponse,
                     "put")},
    {"PUT", "", mojom::FetchCredentialsMode::kOmit, "put", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    // ... But, GET is always allowed by the safe list.
    {"get", "", mojom::FetchCredentialsMode::kOmit, "GET", "",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
};

const TestCase header_cases[] = {
    // Found in the preflight response.
    {"GET", "X-MY-HEADER", mojom::FetchCredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t", mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"GET", "X-MY-HEADER, Y-MY-HEADER", mojom::FetchCredentialsMode::kOmit,
     "GET", "X-MY-HEADER:t\r\nY-MY-HEADER:t",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"GET", "x-my-header, Y-MY-HEADER", mojom::FetchCredentialsMode::kOmit,
     "GET", "X-MY-HEADER:t\r\ny-my-header:t",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // Found in the safe list.
    {"GET", "", mojom::FetchCredentialsMode::kOmit, "GET", "Accept:*/*",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // By '*'.
    {"GET", "*", mojom::FetchCredentialsMode::kOmit, "GET", "xyzzy:t",
     mojom::FetchCredentialsMode::kOmit, base::nullopt},
    {"GET", "*", mojom::FetchCredentialsMode::kInclude, "GET", "xyzzy:t",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kHeaderDisallowedByPreflightResponse,
                     "xyzzy")},

    // Forbidden headers can pass.
    {"GET", "", mojom::FetchCredentialsMode::kOmit, "GET",
     "Host: www.google.com", mojom::FetchCredentialsMode::kOmit, base::nullopt},

    // Not found in the preflight response and the safe list.
    {"GET", "", mojom::FetchCredentialsMode::kOmit, "GET", "X-MY-HEADER:t",
     mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kHeaderDisallowedByPreflightResponse,
                     "x-my-header")},
    {"GET", "X-SOME-OTHER-HEADER", mojom::FetchCredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t", mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kHeaderDisallowedByPreflightResponse,
                     "x-my-header")},
    {"GET", "X-MY-HEADER", mojom::FetchCredentialsMode::kOmit, "GET",
     "X-MY-HEADER:t\r\nY-MY-HEADER:t", mojom::FetchCredentialsMode::kOmit,
     CORSErrorStatus(mojom::CORSError::kHeaderDisallowedByPreflightResponse,
                     "y-my-header")},
};

TEST_F(PreflightResultTest, MaxAge) {
  std::unique_ptr<base::SimpleTestTickClock> tick_clock =
      std::make_unique<base::SimpleTestTickClock>();
  PreflightResult::SetTickClockForTesting(tick_clock.get());

  std::unique_ptr<PreflightResult> result1 =
      PreflightResult::Create(mojom::FetchCredentialsMode::kOmit, base::nullopt,
                              base::nullopt, std::string("573"), nullptr);
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSeconds(573),
            result1->absolute_expiry_time());

  // Negative values are invalid. The preflight result itself can be usable, but
  // should not cache such results. PreflightResult expresses it as a result
  // with 'Access-Control-Max-Age: 0'.
  std::unique_ptr<PreflightResult> result2 =
      PreflightResult::Create(mojom::FetchCredentialsMode::kOmit, base::nullopt,
                              base::nullopt, std::string("-765"), nullptr);
  EXPECT_EQ(base::TimeTicks(), result2->absolute_expiry_time());

  PreflightResult::SetTickClockForTesting(nullptr);
};

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
    const mojom::FetchCredentialsMode cache_credentials_mode;
    const mojom::FetchCredentialsMode request_credentials_mode;
    const bool expected_result;
  } credentials_cases[] = {
      // Different credential modes.
      {mojom::FetchCredentialsMode::kInclude,
       mojom::FetchCredentialsMode::kOmit, true},
      {mojom::FetchCredentialsMode::kInclude,
       mojom::FetchCredentialsMode::kInclude, true},

      // Credential mode mismatch.
      {mojom::FetchCredentialsMode::kOmit, mojom::FetchCredentialsMode::kOmit,
       true},
      {mojom::FetchCredentialsMode::kOmit,
       mojom::FetchCredentialsMode::kInclude, false},
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
