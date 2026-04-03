// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/immediate_request_rate_limiter.h"

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "device/fido/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webauthn {

namespace {

constexpr int kTestMaxRequestsLong = 5;
constexpr int kTestWindowSecondsLong = 60;
constexpr int kTestMaxRequestsShort = 3;
constexpr int kTestWindowSecondsShort = 5;

class ImmediateRequestRateLimiterTest : public ::testing::Test {
 protected:
  ImmediateRequestRateLimiterTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SetRateLimitParams(kTestMaxRequestsLong, kTestWindowSecondsLong,
                       kTestMaxRequestsShort, kTestWindowSecondsShort);
    EnableRateLimitFeature();
  }

  void SetRateLimitParams(int max_requests_long,
                          int window_seconds_long,
                          int max_requests_short,
                          int window_seconds_short) {
    feature_params_[device::kWebAuthnImmediateRequestLongRateLimitMaxRequests
                        .name] = base::NumberToString(max_requests_long);
    feature_params_[device::kWebAuthnImmediateRequestLongRateLimitWindowSeconds
                        .name] = base::NumberToString(window_seconds_long);
    feature_params_[device::kWebAuthnImmediateRequestShortRateLimitMaxRequests
                        .name] = base::NumberToString(max_requests_short);
    feature_params_[device::kWebAuthnImmediateRequestShortRateLimitWindowSeconds
                        .name] = base::NumberToString(window_seconds_short);
  }

  void EnableRateLimitFeature() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        device::kWebAuthnImmediateRequestRateLimit, feature_params_);
  }

  void DisableRateLimitFeature() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        device::kWebAuthnImmediateRequestRateLimit);
  }

  base::test::TaskEnvironment task_environment_;
  ImmediateRequestRateLimiter rate_limiter_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<std::string, std::string> feature_params_;
};

TEST_F(ImmediateRequestRateLimiterTest, FeatureDisabled) {
  DisableRateLimitFeature();
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  // Should always allow requests when the feature is disabled.
  for (int i = 0; i < kTestMaxRequestsLong + 5; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin))
        << "Request should be allowed when feature is disabled (attempt "
        << i + 1 << ")";
  }
}

TEST_F(ImmediateRequestRateLimiterTest, FeatureEnabled_BasicLimitShort) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  // First kTestMaxRequestsShort should be allowed.
  for (int i = 0; i < kTestMaxRequestsShort; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin))
        << "Request should be allowed within limit (attempt " << i + 1 << ")";
  }

  // The next request should be denied.
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin))
      << "Request should be denied after exceeding limit";

  // Advance time slightly less than the window.
  task_environment_.FastForwardBy(base::Seconds(kTestWindowSecondsShort - 1));
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin))
      << "Request should still be denied before window expires";

  // Advance time past the window. This advances to the end of the long window
  // because the previous requests will exceed the long rate limiter as well.
  task_environment_.FastForwardBy(
      base::Seconds(kTestWindowSecondsLong - kTestWindowSecondsShort + 1));
  EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin))
      << "Request should be allowed again after window expires";
}

TEST_F(ImmediateRequestRateLimiterTest, FeatureEnabled_BasicLimitLong) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  // First kTestMaxRequestsShort should be allowed.
  for (int i = 0; i < kTestMaxRequestsShort; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin))
        << "Request should be allowed within limit (attempt " << i + 1 << ")";
  }

  // Advance to the end of the short time window, and use the rest of the
  // allowed requests on the long limiter.
  task_environment_.FastForwardBy(base::Seconds(kTestWindowSecondsShort));
  for (int i = 0; i < kTestMaxRequestsLong - kTestMaxRequestsShort; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin))
        << "Request should be allowed within long time limit (attempt " << i + 1
        << ")";
  }

  // The next request should be denied.
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin))
      << "Request should be denied after exceeding limit";

  // Advance time slightly less than the window.
  task_environment_.FastForwardBy(
      base::Seconds(kTestWindowSecondsLong - kTestWindowSecondsShort - 1));
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin))
      << "Request should still be denied before window expires";

  // Advance time past the window.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin))
      << "Request should be allowed again after window expires";
}

TEST_F(ImmediateRequestRateLimiterTest, FeatureEnabled_SubdomainsShareLimit) {
  const url::Origin origin1 =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin origin2 =
      url::Origin::Create(GURL("https://b.example.com"));
  const url::Origin origin3 = url::Origin::Create(GURL("https://example.com"));

  // Use up the limit with origin1.
  for (int i = 0; i < kTestMaxRequestsShort; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin1))
        << "Request for origin1 should be allowed (attempt " << i + 1 << ")";
  }

  // Next request for origin1 should fail.
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin1))
      << "Request for origin1 should be denied after exceeding limit";

  // Requests for origin2 and origin3 should also fail as they share the eTLD+1.
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin2))
      << "Request for origin2 should be denied (shared limit)";
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin3))
      << "Request for origin3 should be denied (shared limit)";

  // Advance time past the window.
  task_environment_.FastForwardBy(base::Seconds(kTestWindowSecondsLong));

  // All origins should now be allowed again (up to the limit).
  EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin1))
      << "Request for origin1 should be allowed after window expires";
  EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin2))
      << "Request for origin2 should be allowed after window expires";
  EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin3))
      << "Request for origin3 should be allowed after window expires";
}

TEST_F(ImmediateRequestRateLimiterTest, FeatureEnabled_DifferentDomains) {
  const url::Origin origin_com =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin origin_org =
      url::Origin::Create(GURL("https://example.org"));

  // Use up the limit for example.com.
  for (int i = 0; i < kTestMaxRequestsShort; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin_com));
  }
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin_com));

  // example.org should still have its full quota.
  for (int i = 0; i < kTestMaxRequestsShort; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin_org))
        << "Request for origin_org should be allowed (attempt " << i + 1 << ")";
  }
  EXPECT_FALSE(rate_limiter_.IsRequestAllowed(origin_org))
      << "Request for origin_org should be denied after exceeding its limit";
}

TEST_F(ImmediateRequestRateLimiterTest, FeatureEnabled_Localhost) {
  // Localhost doesn't have eTLD+1, should fall back to host.
  const url::Origin origin_localhost =
      url::Origin::Create(GURL("http://localhost:8080"));

  // Should always allow requests for localhost, regardless of limit,
  // because it's explicitly bypassed.
  for (int i = 0; i < kTestMaxRequestsShort + 5; ++i) {
    EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin_localhost))
        << "Request for localhost should always be allowed (attempt " << i + 1
        << ")";
  }

  // Advance time past the window - should still be allowed.
  task_environment_.FastForwardBy(base::Seconds(kTestWindowSecondsLong));
  EXPECT_TRUE(rate_limiter_.IsRequestAllowed(origin_localhost))
      << "Request for localhost should still be allowed after window time";
}

}  // namespace

}  // namespace webauthn
