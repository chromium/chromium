// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ReferrerSanitizerTest = testing::Test;

TEST_F(ReferrerSanitizerTest, SanitizesPolicyForEmptyReferrers) {
  EXPECT_DCHECK_DEATH(ignore_result(Referrer::SanitizeForRequest(
      GURL("https://a"),
      Referrer(GURL(), static_cast<network::mojom::ReferrerPolicy>(200)))));
}

TEST_F(ReferrerSanitizerTest, SanitizesPolicyForNonEmptyReferrers) {
  EXPECT_DCHECK_DEATH(ignore_result(Referrer::SanitizeForRequest(
      GURL("https://a"),
      Referrer(GURL("http://b"),
               static_cast<network::mojom::ReferrerPolicy>(200)))));
}

TEST(ReferrerSanitizerTest, OnlyHTTPFamilyReferrer) {
  auto result = Referrer::SanitizeForRequest(
      GURL("https://a"),
      Referrer(GURL("chrome-extension://ghbmnnjooekpmoecnnnilnnbdlolhkhi"),
               network::mojom::ReferrerPolicy::kAlways));
  EXPECT_TRUE(result.url.is_empty());
}

TEST(ReferrerTest, BlinkNetRoundTripConversion) {
  const net::URLRequest::ReferrerPolicy policies[] = {
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
      net::URLRequest::NEVER_CLEAR_REFERRER,
      net::URLRequest::ORIGIN,
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
      net::URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::URLRequest::NO_REFERRER,
  };

  for (auto policy : policies) {
    EXPECT_EQ(Referrer::ReferrerPolicyForUrlRequest(
                  Referrer::NetReferrerPolicyToBlinkReferrerPolicy(policy)),
              policy);
  }
}

}  // namespace content
