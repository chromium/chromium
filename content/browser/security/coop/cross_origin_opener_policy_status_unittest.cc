// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/coop/cross_origin_opener_policy_status.h"

#include "base/test/scoped_feature_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

using COOP = network::mojom::CrossOriginOpenerPolicyValue;
using CrossOriginOpenerPolicyStatusTest = testing::Test;

TEST(CrossOriginOpenerPolicyStatusTest,
     ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy) {
  struct TestCase {
    COOP coop_from;
    COOP coop_to;
    bool expect_swap_same_origin;
    bool expect_swap_cross_origin;
    bool expect_swap_new_popup;
  } cases[] = {
      // 'unsafe-none' -> *
      {COOP::kUnsafeNone, COOP::kUnsafeNone, false, false, false},
      {COOP::kUnsafeNone, COOP::kSameOrigin, true, true, true},
      {COOP::kUnsafeNone, COOP::kSameOriginPlusCoep, true, true, true},
      {COOP::kUnsafeNone, COOP::kSameOriginAllowPopups, true, true, true},

      // 'same-origin' -> *
      {COOP::kSameOrigin, COOP::kUnsafeNone, true, true, true},
      {COOP::kSameOrigin, COOP::kSameOrigin, false, true, true},
      {COOP::kSameOrigin, COOP::kSameOriginPlusCoep, true, true, true},
      {COOP::kSameOrigin, COOP::kSameOriginAllowPopups, true, true, true},

      // 'same-origin' + COEP -> *
      {COOP::kSameOriginPlusCoep, COOP::kUnsafeNone, true, true, true},
      {COOP::kSameOriginPlusCoep, COOP::kSameOrigin, true, true, true},
      {COOP::kSameOriginPlusCoep, COOP::kSameOriginPlusCoep, false, true, true},
      {COOP::kSameOriginPlusCoep, COOP::kSameOriginAllowPopups, true, true,
       true},

      // 'same-origin-allow-popups' -> *
      {COOP::kSameOriginAllowPopups, COOP::kUnsafeNone, true, true, false},
      {COOP::kSameOriginAllowPopups, COOP::kSameOrigin, true, true, true},
      {COOP::kSameOriginAllowPopups, COOP::kSameOriginPlusCoep, true, true,
       true},
      {COOP::kSameOriginAllowPopups, COOP::kSameOriginAllowPopups, false, true,
       true},
  };
  for (const auto& test : cases) {
    url::Origin A = url::Origin::Create(GURL("https://www.a.com"));
    url::Origin B = url::Origin::Create(GURL("https://www.b.com"));

    SCOPED_TRACE(testing::Message() << "from " << test.coop_from << " to "
                                    << test.coop_to << std::endl);

    // Verify the behavior for two same-origin documents.
    EXPECT_EQ(ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
                  test.coop_from, A,
                  /*is_navigation_from_initial_empty_document=*/false,
                  test.coop_to, A),
              test.expect_swap_same_origin);

    // Verify behavior for two cross-origin documents.
    EXPECT_EQ(ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
                  test.coop_from, A,
                  /*is_navigation_from_initial_empty_document=*/false,
                  test.coop_to, B),
              test.expect_swap_cross_origin);

    // Verify behavior when opening a new cross-origin popup.
    EXPECT_EQ(ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
                  test.coop_from, A,
                  /*is_navigation_from_initial_empty_document=*/true,
                  test.coop_to, B),
              test.expect_swap_new_popup);
  }
}

}  // namespace content
