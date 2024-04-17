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
using Result = CoopSwapResult;
using CrossOriginOpenerPolicyStatusTest = testing::Test;

TEST(CrossOriginOpenerPolicyStatusTest,
     ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy) {
  struct TestCase {
    COOP coop_from;
    COOP coop_to;
    CoopSwapResult expect_swap_same_origin;
    CoopSwapResult expect_swap_cross_origin;
    CoopSwapResult expect_swap_new_popup;
  } cases[] = {
      // 'unsafe-none' -> *
      {COOP::kUnsafeNone, COOP::kUnsafeNone, Result::kNoSwap, Result::kNoSwap,
       Result::kNoSwap},
      {COOP::kUnsafeNone, COOP::kSameOrigin, Result::kSwap, Result::kSwap,
       Result::kSwap},
      {COOP::kUnsafeNone, COOP::kSameOriginPlusCoep, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kUnsafeNone, COOP::kSameOriginAllowPopups, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kUnsafeNone, COOP::kRestrictProperties, Result::kSwapRelated,
       Result::kSwapRelated, Result::kSwapRelated},
      {COOP::kUnsafeNone, COOP::kRestrictPropertiesPlusCoep,
       Result::kSwapRelated, Result::kSwapRelated, Result::kSwapRelated},

      // 'same-origin' -> *
      {COOP::kSameOrigin, COOP::kUnsafeNone, Result::kSwap, Result::kSwap,
       Result::kSwap},
      {COOP::kSameOrigin, COOP::kSameOrigin, Result::kNoSwap, Result::kSwap,
       Result::kSwap},
      {COOP::kSameOrigin, COOP::kSameOriginPlusCoep, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOrigin, COOP::kSameOriginAllowPopups, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOrigin, COOP::kRestrictProperties, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOrigin, COOP::kRestrictPropertiesPlusCoep, Result::kSwap,
       Result::kSwap, Result::kSwap},

      // 'same-origin' + COEP -> *
      {COOP::kSameOriginPlusCoep, COOP::kUnsafeNone, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginPlusCoep, COOP::kSameOrigin, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginPlusCoep, COOP::kSameOriginPlusCoep, Result::kNoSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginPlusCoep, COOP::kSameOriginAllowPopups, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginPlusCoep, COOP::kRestrictProperties, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginPlusCoep, COOP::kRestrictPropertiesPlusCoep,
       Result::kSwap, Result::kSwap, Result::kSwap},

      // 'same-origin-allow-popups' -> *
      {COOP::kSameOriginAllowPopups, COOP::kUnsafeNone, Result::kSwap,
       Result::kSwap, Result::kNoSwap},
      {COOP::kSameOriginAllowPopups, COOP::kSameOrigin, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginAllowPopups, COOP::kSameOriginPlusCoep, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kSameOriginAllowPopups, COOP::kSameOriginAllowPopups,
       Result::kNoSwap, Result::kSwap, Result::kSwap},
      {COOP::kSameOriginAllowPopups, COOP::kRestrictProperties, Result::kSwap,
       Result::kSwap, Result::kSwapRelated},
      {COOP::kSameOriginAllowPopups, COOP::kRestrictPropertiesPlusCoep,
       Result::kSwap, Result::kSwap, Result::kSwapRelated},

      // 'restrict-properties' -> *
      {COOP::kRestrictProperties, COOP::kUnsafeNone, Result::kSwapRelated,
       Result::kSwapRelated, Result::kSwapRelated},
      {COOP::kRestrictProperties, COOP::kSameOrigin, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kRestrictProperties, COOP::kSameOriginPlusCoep, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kRestrictProperties, COOP::kSameOriginAllowPopups, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kRestrictProperties, COOP::kRestrictProperties, Result::kNoSwap,
       Result::kSwapRelated, Result::kSwapRelated},
      {COOP::kRestrictProperties, COOP::kRestrictPropertiesPlusCoep,
       Result::kSwapRelated, Result::kSwapRelated, Result::kSwapRelated},

      // 'restrict-properties' + COEP -> *
      {COOP::kRestrictPropertiesPlusCoep, COOP::kUnsafeNone,
       Result::kSwapRelated, Result::kSwapRelated, Result::kSwapRelated},
      {COOP::kRestrictPropertiesPlusCoep, COOP::kSameOrigin, Result::kSwap,
       Result::kSwap, Result::kSwap},
      {COOP::kRestrictPropertiesPlusCoep, COOP::kSameOriginPlusCoep,
       Result::kSwap, Result::kSwap, Result::kSwap},
      {COOP::kRestrictPropertiesPlusCoep, COOP::kSameOriginAllowPopups,
       Result::kSwap, Result::kSwap, Result::kSwap},
      {COOP::kRestrictPropertiesPlusCoep, COOP::kRestrictProperties,
       Result::kSwapRelated, Result::kSwapRelated, Result::kSwapRelated},
      {COOP::kRestrictPropertiesPlusCoep, COOP::kRestrictPropertiesPlusCoep,
       Result::kNoSwap, Result::kSwapRelated, Result::kSwapRelated},
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
