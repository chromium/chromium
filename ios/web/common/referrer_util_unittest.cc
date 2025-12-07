// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/referrer_util.h"

#include <array>
#include <string_view>

#include "ios/web/public/navigation/referrer.h"
#include "net/url_request/referrer_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

constexpr auto kTestUrls = std::to_array<std::string_view>({
    "",
    "http://user:password@foo.com/a/b/c.html",
    "https://foo.com/d.html#fragment",
    "http://user:password@bar.net/e/f.html#fragment",
    "https://user:password@bar.net/g/h.html",
});

}  // namespace

namespace web {

using ReferrerUtilTest = PlatformTest;

// Tests that no matter what the transition and policy, the result is always
// stripped of things that should not be in a referrer (e.g., passwords).
TEST_F(ReferrerUtilTest, ReferrerSanitization) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : kTestUrls) {
      for (unsigned int policy = 0; policy <= ReferrerPolicyLast; ++policy) {
        Referrer referrer(GURL(source), static_cast<ReferrerPolicy>(policy));
        std::string value =
            ReferrerHeaderValueForNavigation(GURL(dest), referrer);

        EXPECT_EQ(GURL(value).GetAsReferrer().spec(), value);
      }
    }
  }
}

// Tests that the Always policy works as expected.
TEST_F(ReferrerUtilTest, AlwaysPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyAlways);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Everything should have a full referrer.
      EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
    }
  }
}

// Tests that the Default policy works as expected, and matches
// NoReferrerWhenDowngrade.
TEST_F(ReferrerUtilTest, DefaultPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyDefault);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // All but secure->insecure should have a full referrer.
      if (source_url.SchemeIsCryptographic() &&
          !dest_url.SchemeIsCryptographic()) {
        EXPECT_EQ("", value);
      } else {
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      }

      // Default should match NoReferrerWhenDowngrade in all cases.
      referrer.policy = ReferrerPolicyNoReferrerWhenDowngrade;
      EXPECT_EQ(value, ReferrerHeaderValueForNavigation(dest_url, referrer));
    }
  }
}

// Tests that the Never policy works as expected.
TEST_F(ReferrerUtilTest, NeverPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyNever);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // No referrer in any navigation.
      EXPECT_EQ("", value);
    }
  }
}

// Tests that the Origin policy works as expected.
TEST_F(ReferrerUtilTest, OriginPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Origin should be sent in all cases, even secure->insecure.
      EXPECT_EQ(source_url.DeprecatedGetOriginAsURL().spec(), value);
    }
  }
}

// Tests that the OriginWhenCrossOrigin policy works as expected.
TEST_F(ReferrerUtilTest, OriginWhenCrossOriginPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyOriginWhenCrossOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Full URL for the same origin, and origin for all other cases (even
      // secure->insecure).
      if (source_url.DeprecatedGetOriginAsURL() ==
          dest_url.DeprecatedGetOriginAsURL()) {
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      } else {
        EXPECT_EQ(source_url.DeprecatedGetOriginAsURL().spec(), value);
      }
    }
  }
}

// Tests that the same-origin policy works as expected.
TEST_F(ReferrerUtilTest, SameOriginPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicySameOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Full URL for the same origin, and nothing for all other cases.
      if (source_url.DeprecatedGetOriginAsURL() ==
          dest_url.DeprecatedGetOriginAsURL()) {
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      } else {
        EXPECT_EQ(std::string(), value);
      }
    }
  }
}

// Tests that the strict-origin policy works as expected.
TEST_F(ReferrerUtilTest, StrictOriginPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyStrictOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // No referrer when downgrading, and origin otherwise.
      if (source_url.SchemeIsCryptographic() &&
          !dest_url.SchemeIsCryptographic()) {
        EXPECT_EQ("", value);
      } else {
        EXPECT_EQ(source_url.DeprecatedGetOriginAsURL().spec(), value);
      }
    }
  }
}

// Tests that the strict-origin-when-cross-origin policy works as expected.
TEST_F(ReferrerUtilTest, StrictOriginWhenCrossOriginPolicy) {
  for (std::string_view source : kTestUrls) {
    for (std::string_view dest : base::span(kTestUrls).subspan(1u)) {
      GURL source_url(source);
      GURL dest_url(dest);
      Referrer referrer(source_url, ReferrerPolicyStrictOriginWhenCrossOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // No referrer when downgrading, origin when cross-origin but not
      // downgrading, and full referrer otherwise.
      if (source_url.SchemeIsCryptographic() &&
          !dest_url.SchemeIsCryptographic()) {
        EXPECT_EQ("", value);
      } else if (source_url.DeprecatedGetOriginAsURL() ==
                 dest_url.DeprecatedGetOriginAsURL()) {
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      } else {
        EXPECT_EQ(source_url.DeprecatedGetOriginAsURL().spec(), value);
      }
    }
  }
}

// Tests that PolicyForNavigation gives the right values.
TEST_F(ReferrerUtilTest, PolicyForNavigation) {
  // The request and destination URLs are unused in the current implementation,
  // so use a dummy value.
  GURL dummy_url;
  for (unsigned int policy = 0; policy <= ReferrerPolicyLast; ++policy) {
    Referrer referrer(dummy_url, static_cast<ReferrerPolicy>(policy));
    net::ReferrerPolicy net_request_policy =
        PolicyForNavigation(dummy_url, referrer);
    // The test here is deliberately backward from the way the test would
    // intuitively work so that it's structured differently from the code it's
    // testing, and thus less likely to have a copy/paste bug that passes
    // incorrect mappings.
    switch (net_request_policy) {
      case net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
        // This corresponds directly to ReferrerPolicyNoReferrerWhenDowngrade,
        // which is also how Default works on iOS.
        EXPECT_TRUE(policy == ReferrerPolicyDefault ||
                    policy == ReferrerPolicyNoReferrerWhenDowngrade);
        break;
      case net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
        EXPECT_EQ(ReferrerPolicyStrictOriginWhenCrossOrigin, policy);
        break;
      case net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
        EXPECT_EQ(ReferrerPolicyOriginWhenCrossOrigin, policy);
        break;
      case net::ReferrerPolicy::NEVER_CLEAR:
        EXPECT_EQ(ReferrerPolicyAlways, policy);
        break;
      case net::ReferrerPolicy::ORIGIN:
        EXPECT_EQ(ReferrerPolicyOrigin, policy);
        break;
      case net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
        EXPECT_EQ(ReferrerPolicySameOrigin, policy);
        break;
      case net::ReferrerPolicy::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
        EXPECT_EQ(ReferrerPolicyStrictOrigin, policy);
        break;
      case net::ReferrerPolicy::NO_REFERRER:
        EXPECT_EQ(ReferrerPolicyNever, policy);
        break;
    }
  }
}

// Tests that all the strings corresponding to web::ReferrerPolicy values are
// correctly handled.
TEST_F(ReferrerUtilTest, PolicyFromString) {
  // Maps a policy value to a policy name.
  struct PolicyMapping {
    std::string_view name;
    ReferrerPolicy value;
  };

  // Maps policy names to policy values.
  static constexpr auto kPolicyMappings = std::to_array<PolicyMapping>({
      // Policy names
      {
          "unsafe-url",
          ReferrerPolicyAlways,
      },
      {
          "no-referrer-when-downgrade",
          ReferrerPolicyNoReferrerWhenDowngrade,
      },
      {
          "no-referrer",
          ReferrerPolicyNever,
      },
      {
          "origin",
          ReferrerPolicyOrigin,
      },
      {
          "origin-when-cross-origin",
          ReferrerPolicyOriginWhenCrossOrigin,
      },
      {
          "same-origin",
          ReferrerPolicySameOrigin,
      },
      {
          "strict-origin",
          ReferrerPolicyStrictOrigin,
      },
      {
          "strict-origin-when-cross-origin",
          ReferrerPolicyStrictOriginWhenCrossOrigin,
      },

      // Legacy policy names
      {
          "never",
          ReferrerPolicyNever,
      },
      {
          "always",
          ReferrerPolicyAlways,
      },

      // Per the spec, "default" maps to NoReferrerWhenDowngrade; the
      // Default enum value is not actually a spec'd value.
      {
          "default",
          ReferrerPolicyNoReferrerWhenDowngrade,
      },

      // Invalid values maps to Default
      {
          "",
          ReferrerPolicyDefault,
      },
      {
          "made-up",
          ReferrerPolicyDefault,
      },
  });

  std::set<ReferrerPolicy> policies;
  for (const auto& policy_mapping : kPolicyMappings) {
    policies.insert(policy_mapping.value);
    EXPECT_EQ(policy_mapping.value,
              ReferrerPolicyFromString(policy_mapping.name));
  }

  // Check that all policies have been tested.
  EXPECT_EQ(policies.size(), static_cast<size_t>(ReferrerPolicyLast + 1));
}

}  // namespace web
