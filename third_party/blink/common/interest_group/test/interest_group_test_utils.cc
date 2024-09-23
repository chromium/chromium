// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/test/interest_group_test_utils.h"

#include <stddef.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace blink {

namespace {

// Macros are used to keep the field names in the failure output of EXPECT_EQ().
// These should only be used to implement InterestGroupCompare(), and #undef'd
// after.

// Compare `actual` and `expected`, either expecting equality, or non-equality,
// depending on the value of `expect_equals` passed to InterestGroupCompare().
#define IG_COMPARE(actual, expected) \
  if (expect_equals) {               \
    EXPECT_EQ(actual, expected);     \
  } else {                           \
    if (actual != expected) {        \
      found_unequal = true;          \
    }                                \
  }

// Vectors and maps are a special case -- a parameter `func` is used to compare
// individual elements of the std::vector. IG_COMPARE_MAP() supports
// base::flat_map. std::optional-wrapped vectors and maps are also allowed.
//
// NOTE: Do **NOT** pass a lambda literal directly as `func`, as commas in the
// lambda definition may get mishandled by the  preprocessor, and lines get
// concatenated (making debugging harder). Instead, assign the lambda to a
// variable using "auto", then pass that.
#define IG_COMPARE_VEC(actual, expected, func)                              \
  IgCompareVecInternal(#actual, #expected, actual, expected, expect_equals, \
                       found_unequal, func)

#define IG_COMPARE_MAP(actual, expected, func)                              \
  IgCompareMapInternal(#actual, #expected, actual, expected, expect_equals, \
                       found_unequal, func)

// NOTE: Template template parameters could have been used here to match any
// list-like or map-like type, but the downside is they add complexity and can
// overmatch against non-desired types. Since we only use std::vector and
// base::flat_map, it makes sense to just manually implement those types. C++
// concepts might make it easier to be more general here in the future.

// Helper for IG_COMPARE_VEC() -- do not call directly.
//
// Handles plain std::vector instances *not* wrapped in std::optional.
template <typename T, typename Func>
void IgCompareVecInternal(std::string_view a_name,
                          std::string_view b_name,
                          const std::vector<T>& actual,
                          const std::vector<T>& expected,
                          const bool expect_equals,
                          bool& found_unequal,
                          Func f) {
  SCOPED_TRACE(base::StrCat({a_name, " and ", b_name}));
  IG_COMPARE(actual.size(), expected.size());
  if (actual.size() == expected.size()) {
    for (size_t i = 0; i < actual.size(); i++) {
      SCOPED_TRACE(i);
      f(actual[i], expected[i]);
    }
  }
}

// Helper for IG_COMPARE_VEC() -- do not call directly.
//
// Handles plain std::vector instances that *are* wrapped in std::optional.
template <typename T, typename Func>
void IgCompareVecInternal(std::string_view a_name,
                          std::string_view b_name,
                          const std::optional<std::vector<T>>& actual,
                          const std::optional<std::vector<T>>& expected,
                          const bool expect_equals,
                          bool& found_unequal,
                          Func f) {
  SCOPED_TRACE(base::StrCat({a_name, " and ", b_name}));
  IG_COMPARE(actual.has_value(), expected.has_value());
  if (actual && expected) {
    IgCompareVecInternal(a_name, b_name, *actual, *expected, expect_equals,
                         found_unequal, f);
  }
}

// Helper for IG_COMPARE_MAP() -- do not call directly.
//
// Handles plain base::flat_map instances *not* wrapped in std::optional.
template <typename K, typename V, typename Func>
void IgCompareMapInternal(std::string_view a_name,
                          std::string_view b_name,
                          const base::flat_map<K, V>& actual,
                          const base::flat_map<K, V>& expected,
                          const bool expect_equals,
                          bool& found_unequal,
                          Func f) {
  SCOPED_TRACE(base::StrCat({a_name, " and ", b_name}));
  IG_COMPARE(actual.size(), expected.size());
  if (actual.size() == expected.size()) {
    // NOTE: The correctness of this loop construction depends on the fact that
    // base::flat_map stores elements in sorted key order, so if `actual` and
    // `expected` are equal, their keys will have the same iteration order.
    size_t i = 0;
    for (auto a_it = actual.begin(), b_it = expected.begin();
         a_it != actual.end(); a_it++, b_it++, i++) {
      SCOPED_TRACE(i);
      CHECK(b_it != expected.end());
      // Since interest groups must be representable in JSON (for interest group
      // updates), key types must be strings in JSON. In C++, they are typically
      // either std::string, or url::Origin -- both of which support
      // operator==() and operator<<(). So, it's not necessary to have a
      // separate function passed in for comparing key types.
      IG_COMPARE(a_it->first, b_it->first);
      f(a_it->second, b_it->second);
    }
  }
}

// Helper for IG_COMPARE_MAP() -- do not call directly.
//
// Handles plain base::flat_map instances that *are* wrapped in std::optional.
template <typename K, typename V, typename Func>
void IgCompareMapInternal(std::string_view a_name,
                          std::string_view b_name,
                          const std::optional<base::flat_map<K, V>>& actual,
                          const std::optional<base::flat_map<K, V>>& expected,
                          const bool expect_equals,
                          bool& found_unequal,
                          Func f) {
  SCOPED_TRACE(base::StrCat({a_name, " and ", b_name}));
  IG_COMPARE(actual.has_value(), expected.has_value());
  if (actual && expected) {
    IgCompareMapInternal(a_name, b_name, *actual, *expected, expect_equals,
                         found_unequal, f);
  }
}

// Compares all fields and subfields of blink::InterestGroup using the
// IG_COMPARE*() macros implemented above.
//
// Used to implement IgExpectEqualsForTesting() and
// IgExpectNotEqualsForTesting().
//
// Technically `expected` is `not_expected` in the IgExpectNotEqualsForTesting()
// case, but only the `found_unequal` expectation can fail in that case. For the
// IgExpectEqualsForTesting() case, the name `expected` is appropriate for error
// messages.
void InterestGroupCompare(const blink::InterestGroup& actual,
                          const blink::InterestGroup& expected,
                          bool expect_equals) {
  bool found_unequal = false;

  IG_COMPARE(actual.expiry, expected.expiry);
  IG_COMPARE(actual.owner, expected.owner);
  IG_COMPARE(actual.name, expected.name);
  IG_COMPARE(actual.priority, expected.priority);
  IG_COMPARE(actual.enable_bidding_signals_prioritization,
             expected.enable_bidding_signals_prioritization);
  auto compare_doubles = [&](double actual, double expected) {
    IG_COMPARE(actual, expected);
  };
  IG_COMPARE_MAP(actual.priority_vector, expected.priority_vector,
                 compare_doubles);
  IG_COMPARE_MAP(actual.priority_signals_overrides,
                 expected.priority_signals_overrides, compare_doubles);
  auto compare_seller_capabilities = [&](SellerCapabilitiesType actual,
                                         SellerCapabilitiesType expected) {
    IG_COMPARE(actual, expected);
  };
  IG_COMPARE_MAP(actual.seller_capabilities, expected.seller_capabilities,
                 compare_seller_capabilities);
  IG_COMPARE(actual.all_sellers_capabilities,
             expected.all_sellers_capabilities);
  IG_COMPARE(actual.execution_mode, expected.execution_mode);
  IG_COMPARE(actual.bidding_url, expected.bidding_url);
  IG_COMPARE(actual.bidding_wasm_helper_url, expected.bidding_wasm_helper_url);
  IG_COMPARE(actual.update_url, expected.update_url);
  IG_COMPARE(actual.trusted_bidding_signals_url,
             expected.trusted_bidding_signals_url);
  auto compare_strings = [&](const std::string& actual,
                             const std::string& expected) {
    IG_COMPARE(actual, expected);
  };
  IG_COMPARE_VEC(actual.trusted_bidding_signals_keys,
                 expected.trusted_bidding_signals_keys, compare_strings);
  IG_COMPARE(actual.trusted_bidding_signals_slot_size_mode,
             expected.trusted_bidding_signals_slot_size_mode);
  IG_COMPARE(actual.max_trusted_bidding_signals_url_length,
             expected.max_trusted_bidding_signals_url_length);
  IG_COMPARE(actual.trusted_bidding_signals_coordinator,
             expected.trusted_bidding_signals_coordinator);
  IG_COMPARE(actual.user_bidding_signals, expected.user_bidding_signals);
  auto compare_ads = [&](const blink::InterestGroup::Ad& actual,
                         const blink::InterestGroup::Ad& expected) {
    IG_COMPARE(actual.render_url(), expected.render_url());
    IG_COMPARE(actual.size_group, expected.size_group);
    IG_COMPARE(actual.metadata, expected.metadata);
    IG_COMPARE(actual.buyer_reporting_id, expected.buyer_reporting_id);
    IG_COMPARE(actual.buyer_and_seller_reporting_id,
               expected.buyer_and_seller_reporting_id);
    IG_COMPARE_VEC(actual.selectable_buyer_and_seller_reporting_ids,
                   expected.selectable_buyer_and_seller_reporting_ids,
                   compare_strings);
    IG_COMPARE(actual.ad_render_id, expected.ad_render_id);

    auto compare_origins = [&](const url::Origin& actual,
                               const url::Origin& expected) {
      IG_COMPARE(actual, expected);
    };
    IG_COMPARE_VEC(actual.allowed_reporting_origins,
                   expected.allowed_reporting_origins, compare_origins);
  };
  IG_COMPARE_VEC(actual.ads, expected.ads, compare_ads);
  IG_COMPARE_VEC(actual.ad_components, expected.ad_components, compare_ads);
  auto compare_ad_sizes = [&](const blink::AdSize& actual,
                              const blink::AdSize& expected) {
    IG_COMPARE(actual.width, expected.width);
    IG_COMPARE(actual.width_units, expected.width_units);
    IG_COMPARE(actual.height, expected.height);
    IG_COMPARE(actual.height_units, expected.height_units);
  };
  IG_COMPARE_MAP(actual.ad_sizes, expected.ad_sizes, compare_ad_sizes);
  auto compare_vectors_of_strings =
      [&](const std::vector<std::string>& actual,
          const std::vector<std::string>& expected) {
        IG_COMPARE_VEC(actual, expected, compare_strings);
      };
  IG_COMPARE_MAP(actual.size_groups, expected.size_groups,
                 compare_vectors_of_strings);
  IG_COMPARE(actual.auction_server_request_flags,
             expected.auction_server_request_flags);
  IG_COMPARE(actual.additional_bid_key, expected.additional_bid_key);
  IG_COMPARE(actual.aggregation_coordinator_origin,
             expected.aggregation_coordinator_origin);

  if (!expect_equals) {
    EXPECT_TRUE(found_unequal);
  }
}

#undef IG_COMPARE_MAP
#undef IG_COMPARE_VEC
#undef IG_COMPARE

}  // namespace

void IgExpectEqualsForTesting(const blink::InterestGroup& actual,
                              const blink::InterestGroup& expected) {
  InterestGroupCompare(actual, expected, /*expect_equals=*/true);
}

void IgExpectNotEqualsForTesting(const blink::InterestGroup& actual,
                                 const blink::InterestGroup& not_expected) {
  InterestGroupCompare(actual, not_expected, /*expect_equals=*/false);
}

}  // namespace blink
