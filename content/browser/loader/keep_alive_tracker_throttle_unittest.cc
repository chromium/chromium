// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_tracker_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

using testing::IsNull;
using testing::NotNull;

network::ResourceRequest CreateRequest(
    std::string_view url,
    network::mojom::AttributionReportingEligibility attribution_eligibility =
        network::mojom::AttributionReportingEligibility::kEmpty,
    bool keepalive = true) {
  network::ResourceRequest request;

  request.url = GURL(url);
  request.attribution_reporting_eligibility = attribution_eligibility;
  request.keepalive = keepalive;

  return request;
}

class MaybeCreateKeepAliveTrackerThrottleTest : public testing::Test {
 public:
  MaybeCreateKeepAliveTrackerThrottleTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kBeaconLeakageLogging}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MaybeCreateKeepAliveTrackerThrottleTest, NonKeepAliveRequest) {
  auto request = CreateRequest(
      "https://example.com",
      network::mojom::AttributionReportingEligibility::kNavigationSource,
      /*keepalive=*/false);

  EXPECT_THAT(
      KeepAliveTrackerThrottle::MaybeCreateKeepAliveTrackerThrottle(request),
      IsNull());
}

TEST_F(MaybeCreateKeepAliveTrackerThrottleTest, NonCategoryRequest) {
  auto request = CreateRequest(
      "https://example.com",
      network::mojom::AttributionReportingEligibility::kNavigationSource);

  EXPECT_THAT(
      KeepAliveTrackerThrottle::MaybeCreateKeepAliveTrackerThrottle(request),
      IsNull());
}

TEST_F(MaybeCreateKeepAliveTrackerThrottleTest, IsTargetFetchKeepAliveRequest) {
  auto request = CreateRequest("https://example.com?category=123");

  auto throttle =
      KeepAliveTrackerThrottle::MaybeCreateKeepAliveTrackerThrottle(request);
  EXPECT_THAT(throttle, NotNull());
  EXPECT_EQ(throttle->GetRequestTypeForTesting(),
            KeepAliveTrackerThrottle::RequestType::kFetch);
}

TEST_F(MaybeCreateKeepAliveTrackerThrottleTest,
       IsTargetAttributionReportingRequest) {
  auto request = CreateRequest(
      "https://example.com?category=random",
      network::mojom::AttributionReportingEligibility::kNavigationSource);

  auto throttle =
      KeepAliveTrackerThrottle::MaybeCreateKeepAliveTrackerThrottle(request);
  EXPECT_THAT(throttle, NotNull());
  EXPECT_EQ(throttle->GetRequestTypeForTesting(),
            KeepAliveTrackerThrottle::RequestType::kAttribution);
}

}  // namespace
}  // namespace content
