// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/referrer_util.h"

#include "base/stl_util.h"
#include "ios/web/public/navigation/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

const char* const kTestUrls[] = {
    "",
    "http://user:password@foo.com/a/b/c.html",
    "https://foo.com/d.html#fragment",
    "http://user:password@bar.net/e/f.html#fragment",
    "https://user:password@bar.net/g/h.html",
};

}  // namespace

namespace web {

using ReferrerUtilTest = PlatformTest;

// Tests that no matter what the transition and policy, the result is always
// stripped of things that should not be in a referrer (e.g., passwords).
TEST_F(ReferrerUtilTest, ReferrerSanitization) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 0; dest < base::size(kTestUrls); ++dest) {
      for (unsigned int policy = 0; policy <= ReferrerPolicyLast; ++policy) {
        Referrer referrer(GURL(kTestUrls[source]),
                          static_cast<ReferrerPolicy>(policy));
        std::string value =
            ReferrerHeaderValueForNavigation(GURL(kTestUrls[dest]), referrer);

        EXPECT_EQ(GURL(value).GetAsReferrer().spec(), value);
      }
    }
  }
}

// Tests that the Always policy works as expected.
TEST_F(ReferrerUtilTest, AlwaysPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
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
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicyDefault);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // All but secure->insecure should have a full referrer.
      if (source_url.SchemeIsCryptographic() &&
          !dest_url.SchemeIsCryptographic())
        EXPECT_EQ("", value);
      else
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);

      // Default should match NoReferrerWhenDowngrade in all cases.
      referrer.policy = ReferrerPolicyNoReferrerWhenDowngrade;
      EXPECT_EQ(value, ReferrerHeaderValueForNavigation(dest_url, referrer));
    }
  }
}

// Tests that the Never policy works as expected.
TEST_F(ReferrerUtilTest, NeverPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicyNever);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // No referrer in any navigation.
      EXPECT_EQ("", value);
    }
  }
}

// Tests that the Origin policy works as expected.
TEST_F(ReferrerUtilTest, OriginPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicyOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Origin should be sent in all cases, even secure->insecure.
      EXPECT_EQ(source_url.GetOrigin().spec(), value);
    }
  }
}

// Tests that the OriginWhenCrossOrigin policy works as expected.
TEST_F(ReferrerUtilTest, OriginWhenCrossOriginPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicyOriginWhenCrossOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Full URL for the same origin, and origin for all other cases (even
      // secure->insecure).
      if (source_url.GetOrigin() == dest_url.GetOrigin())
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      else
        EXPECT_EQ(source_url.GetOrigin().spec(), value);
    }
  }
}

// Tests that the same-origin policy works as expected.
TEST_F(ReferrerUtilTest, SameOriginPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicySameOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // Full URL for the same origin, and nothing for all other cases.
      if (source_url.GetOrigin() == dest_url.GetOrigin())
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      else
        EXPECT_EQ(std::string(), value);
    }
  }
}

// Tests that the strict-origin policy works as expected.
TEST_F(ReferrerUtilTest, StrictOriginPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicyStrictOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // No referrer when downgrading, and origin otherwise.
      if (source_url.SchemeIsCryptographic() &&
          !dest_url.SchemeIsCryptographic())
        EXPECT_EQ("", value);
      else
        EXPECT_EQ(source_url.GetOrigin().spec(), value);
    }
  }
}

// Tests that the strict-origin-when-cross-origin policy works as expected.
TEST_F(ReferrerUtilTest, StrictOriginWhenCrossOriginPolicy) {
  for (unsigned int source = 0; source < base::size(kTestUrls); ++source) {
    for (unsigned int dest = 1; dest < base::size(kTestUrls); ++dest) {
      GURL source_url(kTestUrls[source]);
      GURL dest_url(kTestUrls[dest]);
      Referrer referrer(source_url, ReferrerPolicyStrictOriginWhenCrossOrigin);
      std::string value = ReferrerHeaderValueForNavigation(dest_url, referrer);

      // No referrer when downgrading, origin when cross-origin but not
      // downgrading, and full referrer otherwise.
      if (source_url.SchemeIsCryptographic() &&
          !dest_url.SchemeIsCryptographic())
        EXPECT_EQ("", value);
      else if (source_url.GetOrigin() == dest_url.GetOrigin())
        EXPECT_EQ(source_url.GetAsReferrer().spec(), value);
      else
        EXPECT_EQ(source_url.GetOrigin().spec(), value);
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
    net::URLRequest::ReferrerPolicy net_request_policy =
        PolicyForNavigation(dummy_url, referrer);
    // The test here is deliberately backward from the way the test would
    // intuitively work so that it's structured differently from the code it's
    // testing, and thus less likely to have a copy/paste bug that passes
    // incorrect mappings.
    switch (net_request_policy) {
      case net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
        // This corresponds directly to ReferrerPolicyNoReferrerWhenDowngrade,
        // which is also how Default works on iOS.
        EXPECT_TRUE(policy == ReferrerPolicyDefault ||
                    policy == ReferrerPolicyNoReferrerWhenDowngrade);
        break;
      case net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
        EXPECT_EQ(ReferrerPolicyStrictOriginWhenCrossOrigin, policy);
        break;
      case net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
        EXPECT_EQ(ReferrerPolicyOriginWhenCrossOrigin, policy);
        break;
      case net::URLRequest::NEVER_CLEAR_REFERRER:
        EXPECT_EQ(ReferrerPolicyAlways, policy);
        break;
      case net::URLRequest::ORIGIN:
        EXPECT_EQ(ReferrerPolicyOrigin, policy);
        break;
      case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN:
        EXPECT_EQ(ReferrerPolicySameOrigin, policy);
        break;
      case net::URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
        EXPECT_EQ(ReferrerPolicyStrictOrigin, policy);
        break;
      case net::URLRequest::NO_REFERRER:
        EXPECT_EQ(ReferrerPolicyNever, policy);
        break;
    }
  }
}

// Tests that all the strings corresponding to web::ReferrerPolicy values are
// correctly handled.
TEST_F(ReferrerUtilTest, PolicyFromString) {
  // The ordering here must match web::ReferrerPolicy; this makes the test
  // simpler, at the cost of needing to re-order if the enum is re-ordered.
  const char* const kPolicyStrings[] = {
      "unsafe-url",
      nullptr,  // Default is skipped, because no string maps to Default.
      "no-referrer-when-downgrade",
      "no-referrer",
      "origin",
      "origin-when-cross-origin",
      "same-origin",
      "strict-origin",
      "strict-origin-when-cross-origin",
  };
  // Test that all the values are supported.
  for (int i = 0; i < ReferrerPolicyLast; ++i) {
    if (!kPolicyStrings[i])
      continue;
    EXPECT_EQ(i, ReferrerPolicyFromString(kPolicyStrings[i]));
  }

  // Verify that if something is added to the enum, its string value gets added
  // to the mapping function.
  EXPECT_EQ(ReferrerPolicyLast + 1,
            static_cast<int>(base::size(kPolicyStrings)));

  // Test the legacy policy names.
  EXPECT_EQ(ReferrerPolicyNever, ReferrerPolicyFromString("never"));
  // Note that per the spec, "default" maps to NoReferrerWhenDowngrade; the
  // Default enum value is not actually a spec'd value.
  EXPECT_EQ(ReferrerPolicyNoReferrerWhenDowngrade,
            ReferrerPolicyFromString("default"));
  EXPECT_EQ(ReferrerPolicyAlways, ReferrerPolicyFromString("always"));

  // Test that invalid values map to Default.
  EXPECT_EQ(ReferrerPolicyDefault, ReferrerPolicyFromString(""));
  EXPECT_EQ(ReferrerPolicyDefault, ReferrerPolicyFromString("made-up"));
}

}  // namespace web
