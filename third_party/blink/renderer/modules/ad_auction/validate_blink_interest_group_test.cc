// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_display_size.mojom-blink.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

constexpr char kOriginString[] = "https://origin.test/";
constexpr char kNameString[] = "name";
constexpr char kCoordinatorOriginString[] = "https://example.test/";

mojom::blink::InterestGroupAdPtr MakeAdWithUrl(const KURL& url) {
  return mojom::blink::InterestGroupAd::New(
      url, /*size_group=*/String(),
      /*buyer_reporting_id=*/String(),
      /*buyer_and_seller_reporting_id=*/String(),
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*metadata=*/String(), /*ad_render_id=*/String(),
      /*allowed_reporting_origins=*/std::nullopt);
}

}  // namespace

// Test fixture for testing both ValidateBlinkInterestGroup() and
// ValidateInterestGroup(), and making sure they behave the same.
class ValidateBlinkInterestGroupTest : public testing::Test {
 public:
  // Check that `blink_interest_group` is valid, if added from its owner origin.
  void ExpectInterestGroupIsValid(
      const mojom::blink::InterestGroupPtr& blink_interest_group) {
    String error_field_name;
    String error_field_value;
    String error;
    EXPECT_TRUE(ValidateBlinkInterestGroup(
        *blink_interest_group, error_field_name, error_field_value, error));
    EXPECT_TRUE(error_field_name.IsNull());
    EXPECT_TRUE(error_field_value.IsNull());
    EXPECT_TRUE(error.IsNull());

    blink::InterestGroup interest_group;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::blink::InterestGroup>(
            blink_interest_group, interest_group));
    EXPECT_EQ(EstimateBlinkInterestGroupSize(*blink_interest_group),
              interest_group.EstimateSize());
  }

  // Check that `blink_interest_group` is not valid, if added from
  // `blink_origin`, and returns the provided error values.
  void ExpectInterestGroupIsNotValid(
      const mojom::blink::InterestGroupPtr& blink_interest_group,
      String expected_error_field_name,
      String expected_error_field_value,
      String expected_error,
      bool check_deserialization = true) {
    String error_field_name;
    String error_field_value;
    String error;
    EXPECT_FALSE(ValidateBlinkInterestGroup(
        *blink_interest_group, error_field_name, error_field_value, error));
    EXPECT_EQ(expected_error_field_name, error_field_name);
    EXPECT_EQ(expected_error_field_value, error_field_value);
    EXPECT_EQ(expected_error, error);

    if (check_deserialization) {
      blink::InterestGroup interest_group;
      // mojo deserialization will call InterestGroup::IsValid.
      EXPECT_FALSE(
          mojo::test::SerializeAndDeserialize<mojom::blink::InterestGroup>(
              blink_interest_group, interest_group));
    }
  }

  // Creates and returns a minimally populated mojom::blink::InterestGroup.
  mojom::blink::InterestGroupPtr CreateMinimalInterestGroup() {
    mojom::blink::InterestGroupPtr blink_interest_group =
        mojom::blink::InterestGroup::New();
    blink_interest_group->owner = kOrigin;
    blink_interest_group->name = kName;
    blink_interest_group->all_sellers_capabilities =
        mojom::blink::SellerCapabilities::New();
    blink_interest_group->auction_server_request_flags =
        mojom::blink::AuctionServerRequestFlags::New();
    return blink_interest_group;
  }

  // Creates an interest group with all fields populated with valid values.
  mojom::blink::InterestGroupPtr CreateFullyPopulatedInterestGroup() {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();

    // Url that's allowed in every field. Populate all portions of the URL that
    // are allowed in most places.
    const KURL kAllowedUrl =
        KURL(String::FromUTF8("https://origin.test/foo?bar"));
    blink_interest_group->bidding_url = kAllowedUrl;
    blink_interest_group->update_url = kAllowedUrl;
    blink_interest_group->bidding_wasm_helper_url = kAllowedUrl;

    // `trusted_bidding_signals_url` doesn't allow query strings, unlike the
    // above ones.
    blink_interest_group->trusted_bidding_signals_url =
        KURL(String::FromUTF8("https://origin.test/foo"));

    blink_interest_group->trusted_bidding_signals_keys.emplace();
    blink_interest_group->trusted_bidding_signals_keys->push_back(
        String::FromUTF8("1"));
    blink_interest_group->trusted_bidding_signals_keys->push_back(
        String::FromUTF8("2"));
    blink_interest_group->max_trusted_bidding_signals_url_length = 8000;
    blink_interest_group->trusted_bidding_signals_coordinator =
        kCoordinatorOrigin;
    blink_interest_group->user_bidding_signals =
        String::FromUTF8("\"This field isn't actually validated\"");

    // Add two ads. Use different URLs, with references.
    blink_interest_group->ads.emplace();
    auto mojo_ad1 = mojom::blink::InterestGroupAd::New();
    mojo_ad1->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?bar#baz"));
    mojo_ad1->metadata =
        String::FromUTF8("\"This field isn't actually validated\"");
    mojo_ad1->ad_render_id = String::FromUTF8("\"NotTooLong\"");
    mojo_ad1->allowed_reporting_origins.emplace();
    mojo_ad1->allowed_reporting_origins->emplace_back(kOrigin);
    blink_interest_group->ads->push_back(std::move(mojo_ad1));
    auto mojo_ad2 = mojom::blink::InterestGroupAd::New();
    mojo_ad2->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?bar#baz2"));
    blink_interest_group->ads->push_back(std::move(mojo_ad2));

    // Add two ad components. Use different URLs, with references.
    blink_interest_group->ad_components.emplace();
    auto mojo_ad_component1 = mojom::blink::InterestGroupAd::New();
    mojo_ad_component1->render_url =
        KURL(String::FromUTF8("https://origin.test/components?bar#baz"));
    mojo_ad_component1->metadata =
        String::FromUTF8("\"This field isn't actually validated\"");
    mojo_ad_component1->ad_render_id = String::FromUTF8("\"NotTooLong\"");
    blink_interest_group->ad_components->push_back(
        std::move(mojo_ad_component1));
    auto mojo_ad_component2 = mojom::blink::InterestGroupAd::New();
    mojo_ad_component2->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?component#baz2"));
    blink_interest_group->ad_components->push_back(
        std::move(mojo_ad_component2));

    blink_interest_group->auction_server_request_flags =
        mojom::blink::AuctionServerRequestFlags::New();
    blink_interest_group->auction_server_request_flags->omit_ads = true;
    blink_interest_group->auction_server_request_flags
        ->omit_user_bidding_signals = true;

    blink_interest_group->aggregation_coordinator_origin = kCoordinatorOrigin;

    return blink_interest_group;
  }

 protected:
  // SecurityOrigin used as the owner in most tests.
  const scoped_refptr<const SecurityOrigin> kOrigin =
      SecurityOrigin::CreateFromString(String::FromUTF8(kOriginString));

  const String kName = String::FromUTF8(kNameString);
  const scoped_refptr<const SecurityOrigin> kCoordinatorOrigin =
      SecurityOrigin::CreateFromString(
          String::FromUTF8(kCoordinatorOriginString));
  test::TaskEnvironment task_environment_;
};

// Test behavior with an InterestGroup with as few fields populated as allowed.
TEST_F(ValidateBlinkInterestGroupTest, MinimallyPopulated) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  ExpectInterestGroupIsValid(blink_interest_group);
}

// Test behavior with an InterestGroup with all fields populated with valid
// values.
TEST_F(ValidateBlinkInterestGroupTest, FullyPopulated) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateFullyPopulatedInterestGroup();
  ExpectInterestGroupIsValid(blink_interest_group);
}

// Make sure that non-HTTPS origins are rejected, both as the frame origin, and
// as the owner. HTTPS frame origins with non-HTTPS owners are currently
// rejected due to origin mismatch, but once sites can add users to 3P interest
// groups, they should still be rejected for being non-HTTPS.
TEST_F(ValidateBlinkInterestGroupTest, NonHttpsOriginRejected) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->owner =
      SecurityOrigin::CreateFromString(String::FromUTF8("http://origin.test/"));
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("owner"),
      /*expected_error_field_value=*/String::FromUTF8("http://origin.test"),
      /*expected_error=*/String::FromUTF8("owner origin must be HTTPS."));

  blink_interest_group->owner =
      SecurityOrigin::CreateFromString(String::FromUTF8("data:,foo"));
  // Data URLs have opaque origins, which are mapped to the string "null".
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("owner"),
      /*expected_error_field_value=*/String::FromUTF8("null"),
      /*expected_error=*/String::FromUTF8("owner origin must be HTTPS."));
}

// Same as NonHttpsOriginRejected, but for `seller_capabilities`.
TEST_F(ValidateBlinkInterestGroupTest,
       NonHttpsOriginRejectedSellerCapabilities) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->seller_capabilities.emplace();
  blink_interest_group->seller_capabilities->insert(
      SecurityOrigin::CreateFromString(
          String::FromUTF8("https://origin.test/")),
      mojom::blink::SellerCapabilities::New());
  blink_interest_group->seller_capabilities->insert(
      SecurityOrigin::CreateFromString(String::FromUTF8("http://origin.test/")),
      mojom::blink::SellerCapabilities::New());
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("sellerCapabilities"),
      /*expected_error_field_value=*/String::FromUTF8("http://origin.test"),
      /*expected_error=*/
      String::FromUTF8("sellerCapabilities origins must all be HTTPS."));

  blink_interest_group->seller_capabilities->clear();
  blink_interest_group->seller_capabilities->insert(
      SecurityOrigin::CreateFromString(String::FromUTF8("data:,foo")),
      mojom::blink::SellerCapabilities::New());
  blink_interest_group->seller_capabilities->insert(
      SecurityOrigin::CreateFromString(
          String::FromUTF8("https://origin.test/")),
      mojom::blink::SellerCapabilities::New());
  // Data URLs have opaque origins, which are mapped to the string "null".
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("sellerCapabilities"),
      /*expected_error_field_value=*/String::FromUTF8("null"),
      /*expected_error=*/
      String::FromUTF8("sellerCapabilities origins must all be HTTPS."));

  blink_interest_group->seller_capabilities->clear();
  blink_interest_group->seller_capabilities->insert(
      SecurityOrigin::CreateFromString(String::FromUTF8("https://origin.test")),
      mojom::blink::SellerCapabilities::New());
  blink_interest_group->seller_capabilities->insert(
      SecurityOrigin::CreateFromString(String::FromUTF8("https://invalid^&")),
      mojom::blink::SellerCapabilities::New());
  // Data URLs have opaque origins, which are mapped to the string "null".
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("sellerCapabilities"),
      /*expected_error_field_value=*/String::FromUTF8("null"),
      /*expected_error=*/
      String::FromUTF8("sellerCapabilities origins must all be HTTPS."));
}

// Check that `bidding_url`, `bidding_wasm_helper_url`, `update_url`, and
// `trusted_bidding_signals_url` must be same-origin and HTTPS.
//
// Ad URLs do not have to be same origin, so they're checked in a different
// test.
//
// TODO(morlovich): Remove checking of trusted signals URLs from here once
// the feature blink::features::kFledgePermitCrossOriginTrustedSignals is
// removed.
TEST_F(ValidateBlinkInterestGroupTest, RejectedUrls) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kFledgePermitCrossOriginTrustedSignals);

  // Strings when each field has a bad URL, copied from cc file.
  const char kBadBiddingUrlError[] =
      "biddingLogicURL must have the same origin as the InterestGroup owner "
      "and have no fragment identifier or embedded credentials.";
  const char kBadBiddingWasmHelperUrlError[] =
      "biddingWasmHelperURL must have the same origin as the InterestGroup "
      "owner and have no fragment identifier or embedded credentials.";
  const char kBadUpdateUrlError[] =
      "updateURL must have the same origin as the InterestGroup owner "
      "and have no fragment identifier or embedded credentials.";
  const char kBadTrustedBiddingSignalsUrlError[] =
      "trustedBiddingSignalsURL must have the same origin as the "
      "InterestGroup owner and have no query string, fragment identifier "
      "or embedded credentials.";

  // Nested URL schemes, like filesystem URLs, are the only cases where a URL
  // being same origin with an HTTPS origin does not imply the URL itself is
  // also HTTPS.
  const KURL kFileSystemUrl =
      KURL(String::FromUTF8("filesystem:https://origin.test/foo"));
  EXPECT_TRUE(
      kOrigin->IsSameOriginWith(SecurityOrigin::Create(kFileSystemUrl).get()));

  const KURL kRejectedUrls[] = {
      // HTTP URLs is rejected: it's both the wrong scheme, and cross-origin.
      KURL(String::FromUTF8("filesystem:http://origin.test/foo")),
      // Cross origin HTTPS URLs are rejected.
      KURL(String::FromUTF8("https://origin2.test/foo")),
      // URL with different ports are cross-origin.
      KURL(String::FromUTF8("https://origin.test:1234/")),
      // URLs with opaque origins are cross-origin.
      KURL(String::FromUTF8("data://text/html,payload")),
      // Unknown scheme.
      KURL(String::FromUTF8("unknown-scheme://foo/")),

      // filesystem URLs are rejected, even if they're same-origin with the page
      // origin.
      kFileSystemUrl,

      // URLs with user/ports are rejected.
      KURL(String::FromUTF8("https://user:pass@origin.test/")),
      // References also aren't allowed, as they aren't sent over HTTP.
      KURL(String::FromUTF8("https://origin.test/#foopy")),
      // Even empty ones.
      KURL(String::FromUTF8("https://origin.test/#")),

      // Invalid URLs.
      KURL(String::FromUTF8("")),
      KURL(String::FromUTF8("invalid url")),
      KURL(String::FromUTF8("https://!@#$%^&*()/")),
      KURL(String::FromUTF8("https://[1::::::2]/")),
      KURL(String::FromUTF8("https://origin%00.test")),
  };

  for (const KURL& rejected_url : kRejectedUrls) {
    SCOPED_TRACE(rejected_url.GetString());

    // Test `bidding_url`.
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->bidding_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/String::FromUTF8("biddingLogicURL"),
        /*expected_error_field_value=*/rejected_url.GetString(),
        /*expected_error=*/String::FromUTF8(kBadBiddingUrlError));

    // Test `bidding_wasm_helper_url`
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->bidding_wasm_helper_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/String::FromUTF8("biddingWasmHelperURL"),
        /*expected_error_field_value=*/rejected_url.GetString(),
        /*expected_error=*/String::FromUTF8(kBadBiddingWasmHelperUrlError));

    // Test `update_url`.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->update_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/String::FromUTF8("updateURL"),
        /*expected_error_field_value=*/rejected_url.GetString(),
        /*expected_error=*/String::FromUTF8(kBadUpdateUrlError));

    // Test `trusted_bidding_signals_url`.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->trusted_bidding_signals_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/
        String::FromUTF8("trustedBiddingSignalsURL"),
        /*expected_error_field_value=*/rejected_url.GetString(),
        /*expected_error=*/String::FromUTF8(kBadTrustedBiddingSignalsUrlError));
  }

  // `trusted_bidding_signals_url` also can't include query strings.
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  KURL rejected_url = KURL(String::FromUTF8("https://origin.test/?query"));
  blink_interest_group->trusted_bidding_signals_url = rejected_url;
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("trustedBiddingSignalsURL"),
      /*expected_error_field_value=*/rejected_url.GetString(),
      /*expected_error=*/String::FromUTF8(kBadTrustedBiddingSignalsUrlError));

  // That includes an empty query string.
  KURL rejected_url2 = KURL(String::FromUTF8("https://origin.test/?"));
  blink_interest_group->trusted_bidding_signals_url = rejected_url2;
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("trustedBiddingSignalsURL"),
      /*expected_error_field_value=*/rejected_url2.GetString(),
      /*expected_error=*/String::FromUTF8(kBadTrustedBiddingSignalsUrlError));
}

// By default, cross-origin trusted signals URL are accepted, but other checks
// still happen.
TEST_F(ValidateBlinkInterestGroupTest,
       CrossOriginTrustedBiddingSignalsUrlPermitted) {
  // Note that cross-origin checks here refer to the group's owner,
  // https://origin.test
  const struct {
    KURL url;
    bool ok = false;
  } kTests[] = {
      // HTTP URLs is rejected: it's wrong scheme.
      {KURL(String::FromUTF8("http://origin.test/foo"))},
      // Cross origin HTTPS URLs are OK with flag on.
      {KURL(String::FromUTF8("https://origin2.test/foo")), /*ok=*/true},
      // URL with different ports are cross-origin.
      {KURL(String::FromUTF8("https://origin.test:1234/")), /*ok=*/true},
      // URLs with opaque origins are cross-origin, but not OK since they're
      // not https.
      {KURL(String::FromUTF8("data://text/html,payload"))},
      // Unknown scheme.
      {KURL(String::FromUTF8("unknown-scheme://foo/"))},

      // filesystem URLs are rejected, even if they're same-origin with the page
      // origin.
      {KURL(String::FromUTF8("filesystem:https://origin.test/foo"))},

      // URLs with user/ports are rejected.
      {KURL(String::FromUTF8("https://user:pass@origin.test/"))},
      // References also aren't allowed, as they aren't sent over HTTP.
      {KURL(String::FromUTF8("https://origin.test/#foopy"))},
      // Even empty ones.
      {KURL(String::FromUTF8("https://origin.test/#"))},

      // Invalid URLs.
      {KURL(String::FromUTF8(""))},
      {KURL(String::FromUTF8("invalid url"))},
      {KURL(String::FromUTF8("https://!@#$%^&*()/"))},
      {KURL(String::FromUTF8("https://[1::::::2]/"))},
      {KURL(String::FromUTF8("https://origin%00.test"))},

      // `trusted_bidding_signals_url` also can't include query strings.
      {KURL(String::FromUTF8("https://origin.test/?query"))},

      // That includes an empty query string.
      {KURL(String::FromUTF8("https://origin.test/?"))}};

  for (const auto& test : kTests) {
    const KURL& test_url = test.url;
    SCOPED_TRACE(test_url.GetString());
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->trusted_bidding_signals_url = test_url;
    if (test.ok) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          /*expected_error_field_name=*/
          String::FromUTF8("trustedBiddingSignalsURL"),
          /*expected_error_field_value=*/test_url.GetString(),
          /*expected_error=*/
          String::FromUTF8(
              "trustedBiddingSignalsURL must have https schema and have no "
              "query string, fragment identifier or embedded credentials."));
    }
  }
}

// Tests valid and invalid ad render URLs.
TEST_F(ValidateBlinkInterestGroupTest, AdRenderUrlValidation) {
  const char kBadAdUrlError[] =
      "renderURLs must be HTTPS and have no embedded credentials.";

  const struct {
    bool expect_allowed;
    const char* url;
  } kTestCases[] = {
      // Same origin URLs are allowed.
      {true, "https://origin.test/foo?bar"},

      // Cross origin URLs are allowed, as long as they're HTTPS.
      {true, "https://b.test/"},
      {true, "https://a.test:1234/"},

      // URLs with %00 escaped path are allowed.
      {true, "https://origin.test/%00"},

      // URLs with the wrong scheme are rejected.
      {false, "http://a.test/"},
      {false, "data://text/html,payload"},
      {false, "filesystem:https://a.test/foo"},
      {false, "blob:https://a.test:/2987fb0b-034b-4c79-85ae-cc6d3ef9c56e"},
      {false, "about:blank"},
      {false, "about:srcdoc"},
      {false, "about:newtab"},
      {false, "chrome:hang"},

      // URLs with user/ports are rejected.
      {false, "https://user:pass@a.test/"},

      // References are allowed for ads, though not other requests, since they
      // only have an effect when loading a page in a renderer.
      {true, "https://a.test/#foopy"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.url);

    KURL test_case_url = KURL(String::FromUTF8(test_case.url));

    // Add an InterestGroup with the test cases's URL as the only ad's URL.
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->ads.emplace();
    blink_interest_group->ads->emplace_back(MakeAdWithUrl(test_case_url));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          /*expected_error_field_name=*/String::FromUTF8("ads[0].renderURL"),
          /*expected_error_field_value=*/test_case_url.GetString(),
          /*expected_error=*/String::FromUTF8(kBadAdUrlError));
    }

    // Add an InterestGroup with the test cases's URL as the second ad's URL.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->ads.emplace();
    blink_interest_group->ads->emplace_back(
        MakeAdWithUrl(KURL(String::FromUTF8("https://origin.test/"))));
    blink_interest_group->ads->emplace_back(MakeAdWithUrl(test_case_url));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          /*expected_error_field_name=*/String::FromUTF8("ads[1].renderURL"),
          /*expected_error_field_value=*/test_case_url.GetString(),
          /*expected_error=*/String::FromUTF8(kBadAdUrlError));
    }
  }
}

// Tests valid and invalid ad render URLs.
TEST_F(ValidateBlinkInterestGroupTest, AdComponentRenderUrlValidation) {
  const char kBadAdUrlError[] =
      "renderURLs must be HTTPS and have no embedded credentials.";

  const struct {
    bool expect_allowed;
    const char* url;
  } kTestCases[] = {
      // Same origin URLs are allowed.
      {true, "https://origin.test/foo?bar"},

      // Cross origin URLs are allowed, as long as they're HTTPS.
      {true, "https://b.test/"},
      {true, "https://a.test:1234/"},

      // URLs with %00 escaped path are allowed.
      {true, "https://origin.test/%00"},

      // URLs with the wrong scheme are rejected.
      {false, "http://a.test/"},
      {false, "data://text/html,payload"},
      {false, "filesystem:https://a.test/foo"},
      {false, "blob:https://a.test:/2987fb0b-034b-4c79-85ae-cc6d3ef9c56e"},
      {false, "about:blank"},
      {false, "about:srcdoc"},
      {false, "about:newtab"},
      {false, "chrome:hang"},

      // URLs with user/ports are rejected.
      {false, "https://user:pass@a.test/"},

      // References are allowed for ads, though not other requests, since they
      // only have an effect when loading a page in a renderer.
      {true, "https://a.test/#foopy"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.url);

    KURL test_case_url = KURL(String::FromUTF8(test_case.url));

    // Add an InterestGroup with the test cases's URL as the only ad
    // component's URL.
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->ad_components.emplace();
    blink_interest_group->ad_components->emplace_back(
        MakeAdWithUrl(test_case_url));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          /*expected_error_field_name=*/
          String::FromUTF8("adComponents[0].renderURL"),
          /*expected_error_field_value=*/test_case_url.GetString(),
          /*expected_error=*/String::FromUTF8(kBadAdUrlError));
    }

    // Add an InterestGroup with the test cases's URL as the second ad
    // component's URL.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->ad_components.emplace();
    blink_interest_group->ad_components->emplace_back(
        MakeAdWithUrl(KURL(String::FromUTF8("https://origin.test/"))));
    blink_interest_group->ad_components->emplace_back(
        MakeAdWithUrl(test_case_url));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          /*expected_error_field_name=*/
          String::FromUTF8("adComponents[1].renderURL"),
          /*expected_error_field_value=*/test_case_url.GetString(),
          /*expected_error=*/String::FromUTF8(kBadAdUrlError));
    }
  }
}

// Mojo rejects malformed URLs when converting mojom::blink::InterestGroup to
// blink::InterestGroup. Since the rejection happens internally in Mojo,
// typemapping code that invokes blink::InterestGroup::IsValid() isn't run, so
// adding a AdRenderUrlValidation testcase to verify malformed URLs wouldn't
// exercise blink::InterestGroup::IsValid(). Since blink::InterestGroup users
// can call IsValid() directly (i.e when not using Mojo), we need a test that
// also calls IsValid() directly.
TEST_F(ValidateBlinkInterestGroupTest, MalformedUrl) {
  constexpr char kMalformedUrl[] = "https://invalid^";

  // First, check against mojom::blink::InterestGroup.
  constexpr char kBadAdUrlError[] =
      "renderURLs must be HTTPS and have no embedded credentials.";
  mojom::blink::InterestGroupPtr blink_interest_group =
      mojom::blink::InterestGroup::New();
  blink_interest_group->owner = kOrigin;
  blink_interest_group->name = kName;
  blink_interest_group->ads.emplace();
  blink_interest_group->ads->emplace_back(MakeAdWithUrl(KURL(kMalformedUrl)));
  String error_field_name;
  String error_field_value;
  String error;
  EXPECT_FALSE(ValidateBlinkInterestGroup(
      *blink_interest_group, error_field_name, error_field_value, error));
  EXPECT_EQ(error_field_name, String::FromUTF8("ads[0].renderURL"));
  // The invalid ^ gets escaped.
  EXPECT_EQ(error_field_value, String::FromUTF8("https://invalid%5E/"));
  EXPECT_EQ(error, String::FromUTF8(kBadAdUrlError));

  // Now, test against blink::InterestGroup.
  blink::InterestGroup interest_group;
  interest_group.owner = url::Origin::Create(GURL(kOriginString));
  interest_group.name = kNameString;
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(
      blink::InterestGroup::Ad(GURL(kMalformedUrl), /*metadata=*/""));
  EXPECT_FALSE(interest_group.IsValid());
}

TEST_F(ValidateBlinkInterestGroupTest, TooLarge) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();

  // Name length that will result in a `blink_interest_group` having an
  // estimated size of exactly `kMaxInterestGroupSize`, which is 1048576 bytes.
  // Note that kMaxInterestGroupSize is actually one greater than the maximum
  // size, so no need to add 1 to exceed it.
  blink_interest_group->name = "";
  const size_t kTooLongNameLength =
      mojom::blink::kMaxInterestGroupSize -
      EstimateBlinkInterestGroupSize(*blink_interest_group);

  std::string long_string(kTooLongNameLength, 'n');
  blink_interest_group->name = String(long_string);
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too long should still work.
  long_string = std::string(kTooLongNameLength - 1, 'n');
  blink_interest_group->name = String(long_string);
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargePriorityVector) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name = "";

  size_t initial_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  blink_interest_group->priority_vector.emplace();
  // Set 510 entries with 92-byte keys.  Adding in the 8 byte double values,
  // this should be estimated to be 51000 bytes.
  for (int i = 0; i < 510; ++i) {
    // Use a unique 92-byte value for each key.
    String key = String::FromUTF8(base::StringPrintf("%92i", i));
    blink_interest_group->priority_vector->Set(key, i);
  }
  size_t current_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  EXPECT_EQ(51000 + initial_estimate, current_estimate);

  // Name that should cause the group to exactly exceed the maximum name length.
  // Need to call into ExpectInterestGroupIsNotValid() to make sure name length
  // estimate for mojom::blink::InterestGroupPtr and blink::InterestGroup
  // equivalent values exactly match.
  const size_t kTooLongNameLength =
      mojom::blink::kMaxInterestGroupSize - current_estimate;
  std::string too_long_name(kTooLongNameLength, 'n');
  blink_interest_group->name = String(too_long_name);

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too long should still work.
  too_long_name = std::string(kTooLongNameLength - 1, 'n');
  blink_interest_group->name = String(too_long_name);
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargePrioritySignalsOverride) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name = "";

  size_t initial_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  blink_interest_group->priority_signals_overrides.emplace();
  // Set 510 entries with 92-byte keys.  Adding in the 8 byte double values,
  // this should be estimated to be 51000 bytes.
  for (int i = 0; i < 510; ++i) {
    // Use a unique 92-byte value for each key.
    String key = String::FromUTF8(base::StringPrintf("%92i", i));
    blink_interest_group->priority_signals_overrides->Set(key, i);
  }
  size_t current_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  EXPECT_EQ(51000 + initial_estimate, current_estimate);

  // Name that should cause the group to exactly exceed the maximum name length.
  // Need to call into ExpectInterestGroupIsNotValid() to make sure name length
  // estimate for mojom::blink::InterestGroupPtr and blink::InterestGroup
  // equivalent values exactly match.
  const size_t kTooLongNameLength =
      mojom::blink::kMaxInterestGroupSize - current_estimate;
  std::string too_long_name(kTooLongNameLength, 'n');
  blink_interest_group->name = String(too_long_name);

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too long should still work.
  too_long_name = std::string(kTooLongNameLength - 1, 'n');
  blink_interest_group->name = String(too_long_name);
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargeSellerCapabilities) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name = "";

  size_t initial_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  blink_interest_group->seller_capabilities.emplace();
  // Set 510 entries with 100-byte origin values. This should be estimated to be
  // 51000 bytes.
  for (int i = 0; i < 510; ++i) {
    // Use a unique 100-byte value for each origin -- 8 bytes for the
    // "https://", 5 bytes for the ".test" suffix, 4 bytes for the flags, and
    // 100 - 8 - 5 - 4 = 83 bytes of numerical characters.
    String origin_string =
        String::FromUTF8(base::StringPrintf("https://%.83i.test", i));
    blink_interest_group->seller_capabilities->insert(
        SecurityOrigin::CreateFromString(origin_string),
        mojom::blink::SellerCapabilities::New());
  }
  size_t current_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  EXPECT_EQ(51000 + initial_estimate, current_estimate);

  // Name that should cause the group to exactly exceed the maximum name length.
  // Need to call into ExpectInterestGroupIsNotValid() to make sure name length
  // estimate for mojom::blink::InterestGroupPtr and blink::InterestGroup
  // equivalent values exactly match.
  const size_t kTooLongNameLength =
      mojom::blink::kMaxInterestGroupSize - current_estimate;
  std::string too_long_name(kTooLongNameLength, 'n');
  blink_interest_group->name = String(too_long_name);

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too long should still work.
  too_long_name = std::string(kTooLongNameLength - 1, 'n');
  blink_interest_group->name = String(too_long_name);
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargeAdSizes) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name = "";

  size_t initial_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  blink_interest_group->ad_sizes.emplace();
  // Set 510 entries with 100-byte origin values. This should be estimated to be
  // 51000 bytes.
  for (int i = 0; i < 510; ++i) {
    // Use a unique 100-byte value for each name -- 5 bytes for the
    // "size ", and 100 - 8 - 8 - 4 - 4 - 5 = 71 bytes of numerical characters,
    // where 8 is the size of the each double value in the size, 4 is the
    // size of the length unit, and 5 is the length of the string "size ".
    String name_string = String::FromUTF8(base::StringPrintf("size %.71i", i));
    blink_interest_group->ad_sizes->insert(
        name_string,
        mojom::blink::AdSize::New(150, blink::AdSize::LengthUnit::kPixels, 100,
                                  blink::AdSize::LengthUnit::kPixels));
  }
  size_t current_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  EXPECT_EQ(51000 + initial_estimate, current_estimate);

  // Name that should cause the group to exactly exceed the maximum name length.
  // Need to call into ExpectInterestGroupIsNotValid() to make sure name length
  // estimate for mojom::blink::InterestGroupPtr and blink::InterestGroup
  // equivalent values exactly match.
  const size_t kTooLongNameLength =
      mojom::blink::kMaxInterestGroupSize - current_estimate;
  std::string too_long_name(kTooLongNameLength, 'n');
  blink_interest_group->name = String(too_long_name);

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too long should still work.
  too_long_name = std::string(kTooLongNameLength - 1, 'n');
  blink_interest_group->name = String(too_long_name);
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargeSizeGroups) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name = "";

  // There must be at least 1 ad size for the size groups to map to.
  blink_interest_group->ad_sizes.emplace();
  blink_interest_group->ad_sizes->insert(
      "size1", blink::mojom::blink::AdSize::New(
                   100, blink::AdSize::LengthUnit::kPixels, 100,
                   blink::AdSize::LengthUnit::kPixels));

  size_t initial_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);

  blink_interest_group->size_groups.emplace();
  // Set 510 entries with 100-byte origin values. This should be estimated to be
  // 51000 bytes.
  for (int i = 0; i < 510; ++i) {
    // Use a unique 100-byte value for each name -- 6 bytes for the
    // "group ", and 100 - 6 - 5 = 89 bytes of numerical characters, where the 5
    // represents the length of the 1 size name being stored in the vector.
    String name_string = String::FromUTF8(base::StringPrintf("group %.89i", i));
    blink_interest_group->size_groups->insert(
        name_string, WTF::Vector<WTF::String>{"size1"});
  }
  size_t current_estimate =
      EstimateBlinkInterestGroupSize(*blink_interest_group);
  EXPECT_EQ(51000 + initial_estimate, current_estimate);

  // Name that should cause the group to exactly exceed the maximum name length.
  // Need to call into ExpectInterestGroupIsNotValid() to make sure name length
  // estimate for mojom::blink::InterestGroupPtr and blink::InterestGroup
  // equivalent values exactly match.
  const size_t kTooLongNameLength =
      mojom::blink::kMaxInterestGroupSize - current_estimate;
  std::string too_long_name(kTooLongNameLength, 'n');
  blink_interest_group->name = String(too_long_name);

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too long should still work.
  too_long_name = std::string(kTooLongNameLength - 1, 'n');
  blink_interest_group->name = String(too_long_name);
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargeAds) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name =
      WTF::String("paddingTo1048576" + std::string(12, 'P'));
  blink_interest_group->ad_components.emplace();
  for (int i = 0; i < 13980; ++i) {
    // Each ad component is 75 bytes.
    auto mojo_ad_component1 = mojom::blink::InterestGroupAd::New();
    mojo_ad_component1->render_url =
        KURL(String::FromUTF8("https://origin.test/components?bar#baz"));
    mojo_ad_component1->metadata =
        String::FromUTF8("\"This field isn't actually validated\"");
    blink_interest_group->ad_components->push_back(
        std::move(mojo_ad_component1));
  }
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("size"),
      /*expected_error_field_value=*/String::FromUTF8("1048576"),
      /*expected_error=*/
      String::FromUTF8("interest groups must be less than 1048576 bytes"));

  // Almost too big should still work.
  blink_interest_group->ad_components->resize(13979);

  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, InvalidAdSizes) {
  constexpr char kSizeError[] =
      "Ad sizes must have a valid (non-zero/non-infinite) width and height.";
  constexpr char kNameError[] = "Ad sizes cannot map from an empty event name.";
  constexpr char kUnitError[] =
      "Ad size dimensions must be a valid number either in pixels (px) "
      "or screen width (sw).";
  struct {
    const char* ad_name;
    const double width;
    const blink::AdSize::LengthUnit width_units;
    const double height;
    const blink::AdSize::LengthUnit height_units;
    const char* expected_error;
    const char* expected_error_field_value;
  } test_cases[] = {
      {"ad_name", 0, blink::AdSize::LengthUnit::kPixels, 0,
       blink::AdSize::LengthUnit::kPixels, kSizeError, "0.000000 x 0.000000"},
      {"ad_name", 300, blink::AdSize::LengthUnit::kPixels, 0,
       blink::AdSize::LengthUnit::kPixels, kSizeError, "300.000000 x 0.000000"},
      {"ad_name", 0, blink::AdSize::LengthUnit::kScreenWidth, 300,
       blink::AdSize::LengthUnit::kScreenWidth, kSizeError,
       "0.000000 x 300.000000"},
      {"ad_name", -300, blink::AdSize::LengthUnit::kScreenWidth, 300,
       blink::AdSize::LengthUnit::kPixels, kSizeError,
       "-300.000000 x 300.000000"},
      {"", 300, blink::AdSize::LengthUnit::kScreenWidth, 300,
       blink::AdSize::LengthUnit::kPixels, kNameError, ""},
      {"ad_name", std::numeric_limits<double>::infinity(),
       blink::AdSize::LengthUnit::kPixels,
       std::numeric_limits<double>::infinity(),
       blink::AdSize::LengthUnit::kPixels, kSizeError, "inf x inf"},
      {"ad_name", 300, blink::AdSize::LengthUnit::kInvalid, 300,
       blink::AdSize::LengthUnit::kPixels, kUnitError, ""},
  };
  for (const auto& test_case : test_cases) {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->ad_sizes.emplace();
    blink_interest_group->ad_sizes->insert(
        test_case.ad_name, blink::mojom::blink::AdSize::New(
                               test_case.width, test_case.width_units,
                               test_case.height, test_case.height_units));
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/String::FromUTF8("adSizes"),
        test_case.expected_error_field_value, test_case.expected_error);
  }
}

TEST_F(ValidateBlinkInterestGroupTest, InvalidSizeGroups) {
  struct {
    const char* size_group;
    const char* size_name;
    const bool has_ad_sizes;
    const char* expected_error_field_value;
    const char* expected_error;
  } test_cases[] = {
      {"group_name", "", true, "",
       "Size groups cannot map to an empty ad size name."},
      {"", "size_name", true, "",
       "Size groups cannot map from an empty group name."},
      {"group_name", "nonexistant", true, "nonexistant",
       "Size does not exist in adSizes map."},
      {"group_name", "size_name", false, "",
       "An adSizes map must exist for sizeGroups to work."},
  };
  for (const auto& test_case : test_cases) {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    if (test_case.has_ad_sizes) {
      blink_interest_group->ad_sizes.emplace();
      blink_interest_group->ad_sizes->insert(
          "size_name", blink::mojom::blink::AdSize::New(
                           300, blink::AdSize::LengthUnit::kPixels, 150,
                           blink::AdSize::LengthUnit::kPixels));
    }
    blink_interest_group->size_groups.emplace();
    blink_interest_group->size_groups->insert(
        test_case.size_group, WTF::Vector<WTF::String>(1, test_case.size_name));
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/String::FromUTF8("sizeGroups"),
        test_case.expected_error_field_value, test_case.expected_error);
  }
}

TEST_F(ValidateBlinkInterestGroupTest, AdSizeGroupEmptyNameOrNotInSizeGroups) {
  constexpr char kSizeGroupError[] =
      "The assigned size group does not exist in sizeGroups map.";
  constexpr char kNameError[] = "Size group name cannot be empty.";
  struct {
    const char* ad_size_group;
    const char* size_group;
    const char* expected_error_field_value;
    const char* expected_error;
  } test_cases[] = {
      {"", "group_name", "", kNameError},
      {"group_name", "different_group_name", "group_name", kSizeGroupError},
      {"group_name", "", "group_name", kSizeGroupError},
  };
  for (const auto& test_case : test_cases) {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->ads.emplace();
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        KURL("https://origin.test/foo?bar"),
        /*size_group=*/test_case.ad_size_group,
        /*buyer_reporting_id=*/String(),
        /*buyer_and_seller_reporting_id=*/String(),
        /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
        /*metadata=*/String(), /*ad_render_id=*/String(),
        /*allowed_reporting_origins=*/std::nullopt));
    blink_interest_group->ad_sizes.emplace();
    blink_interest_group->ad_sizes->insert(
        "size_name", blink::mojom::blink::AdSize::New(
                         300, blink::AdSize::LengthUnit::kPixels, 150,
                         blink::AdSize::LengthUnit::kPixels));
    blink_interest_group->size_groups.emplace();
    blink_interest_group->size_groups->insert(
        test_case.size_group, WTF::Vector<WTF::String>(1, "size_name"));
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        /*expected_error_field_name=*/String::FromUTF8("ads[0].sizeGroup"),
        test_case.expected_error_field_value, test_case.expected_error);
  }
}

TEST_F(ValidateBlinkInterestGroupTest,
       AdComponentSizeGroupEmptyNameOrNotInSizeGroups) {
  constexpr char kSizeGroupError[] =
      "The assigned size group does not exist in sizeGroups map.";
  constexpr char kNameError[] = "Size group name cannot be empty.";
  struct {
    const char* ad_component_size_group;
    const char* size_group;
    const char* expected_error_field_value;
    const char* expected_error;
  } test_cases[] = {
      {"", "group_name", "", kNameError},
      {"group_name", "different_group_name", "group_name", kSizeGroupError},
      {"group_name", "", "group_name", kSizeGroupError},
  };
  for (const auto& test_case : test_cases) {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->ad_components.emplace();
    blink_interest_group->ad_components->emplace_back(
        mojom::blink::InterestGroupAd::New(
            KURL("https://origin.test/foo?bar"),
            /*size_group=*/test_case.ad_component_size_group,
            /*buyer_reporting_id=*/String(),
            /*buyer_and_seller_reporting_id=*/String(),
            /*selectable_buyer_and_seller_reporting_id=*/std::nullopt,
            /*metadata=*/String(), /*ad_render_id=*/String(),
            /*allowed_reporting_origins=*/std::nullopt));
    blink_interest_group->ad_sizes.emplace();
    blink_interest_group->ad_sizes->insert(
        "size_name", blink::mojom::blink::AdSize::New(
                         300, blink::AdSize::LengthUnit::kPixels, 150,
                         blink::AdSize::LengthUnit::kPixels));
    blink_interest_group->size_groups.emplace();
    blink_interest_group->size_groups->insert(
        test_case.size_group, WTF::Vector<WTF::String>(1, "size_name"));
    ExpectInterestGroupIsNotValid(blink_interest_group,
                                  /*expected_error_field_name=*/
                                  String::FromUTF8("adComponents[0].sizeGroup"),
                                  test_case.expected_error_field_value,
                                  test_case.expected_error);
  }
}

TEST_F(ValidateBlinkInterestGroupTest, AdRenderIdTooLong) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->ads.emplace();
  auto ad = mojom::blink::InterestGroupAd::New();
  ad->render_url = KURL(String::FromUTF8("https://origin.test/foo?bar"));
  ad->ad_render_id = String::FromUTF8("ThisIsTooLong");
  blink_interest_group->ads->emplace_back(std::move(ad));
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("ads[0].adRenderId"),
      /*expected_error_field_value=*/String::FromUTF8("ThisIsTooLong"),
      /*expected_error=*/String::FromUTF8("The adRenderId is too long."));
}

TEST_F(ValidateBlinkInterestGroupTest, AdComponentRenderIdTooLong) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->ad_components.emplace();
  auto mojo_ad_component = mojom::blink::InterestGroupAd::New();
  mojo_ad_component->render_url =
      KURL(String::FromUTF8("https://origin.test/foo?bar"));
  mojo_ad_component->ad_render_id = String::FromUTF8("ThisIsTooLong");
  blink_interest_group->ad_components->emplace_back(
      std::move(mojo_ad_component));
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("adComponents[0].adRenderId"),
      /*expected_error_field_value=*/String::FromUTF8("ThisIsTooLong"),
      /*expected_error=*/String::FromUTF8("The adRenderId is too long."));
}

// The interest group is invalid if its ad object's "allowedReporting" field
// have more than `kMaxAllowedReportingOrigins` elements.
TEST_F(ValidateBlinkInterestGroupTest, AdTooManyAllowedReportingOrigins) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->ads.emplace();
  auto ad = mojom::blink::InterestGroupAd::New();
  ad->render_url = KURL(String::FromUTF8("https://origin.test/foo?bar"));
  ad->allowed_reporting_origins.emplace();
  for (size_t i = 0; i < mojom::blink::kMaxAllowedReportingOrigins + 1; ++i) {
    ad->allowed_reporting_origins->emplace_back(
        SecurityOrigin::CreateFromString(
            String::Format("https://origin%zu.test/", i)));
  }
  blink_interest_group->ads->emplace_back(std::move(ad));
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("ads[0].allowedReportingOrigins"),
      /*expected_error_field_value=*/String::FromUTF8(""),
      /*expected_error=*/
      "allowedReportingOrigins cannot have more than 10 elements.");
}

TEST_F(ValidateBlinkInterestGroupTest, AdNonHttpsAllowedReportingOrigins) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->ads.emplace();
  auto ad = mojom::blink::InterestGroupAd::New();
  ad->render_url = KURL(String::FromUTF8("https://origin.test/foo?bar"));
  ad->allowed_reporting_origins.emplace();
  ad->allowed_reporting_origins->emplace_back(
      SecurityOrigin::CreateFromString("https://origin1.test/"));
  ad->allowed_reporting_origins->emplace_back(
      SecurityOrigin::CreateFromString("http://origin2.test/"));
  blink_interest_group->ads->emplace_back(std::move(ad));
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("ads[0].allowedReportingOrigins"),
      /*expected_error_field_value=*/String::FromUTF8("http://origin2.test"),
      /*expected_error=*/
      String::FromUTF8("allowedReportingOrigins must all be HTTPS."));
}

// Test behavior with a negative InterestGroup.
TEST_F(ValidateBlinkInterestGroupTest, JustAdditionalBidKeyIsValid) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->additional_bid_key = {
      0x7d, 0x4d, 0x0e, 0x7f, 0x61, 0x53, 0xa6, 0x9b, 0x62, 0x42, 0xb5,
      0x22, 0xab, 0xbe, 0xe6, 0x85, 0xfd, 0xa4, 0x42, 0x0f, 0x88, 0x34,
      0xb1, 0x08, 0xc3, 0xbd, 0xae, 0x36, 0x9e, 0xf5, 0x49, 0xfa};
  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, AdditionalBidKeyWrongSize) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->additional_bid_key = {0x7d, 0x4d, 0x0e, 0x7f, 0x61};

  // We specifically don't check deserialization because that would cause a
  // LOG(FATAL), because the fixed-size additional_bid_key array would not be
  // of the expected size.
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String::FromUTF8("additionalBidKey"),
      /*expected_error_field_value=*/String::FromUTF8("5"),
      /*expected_error=*/
      String::FromUTF8("additionalBidKey must be exactly 32 bytes."),
      /*check_deserialization=*/false);
}

TEST_F(ValidateBlinkInterestGroupTest,
       AdditionalBidKeyAndAdsNotAllowedTogether) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->ads.emplace();
  blink_interest_group->ads->emplace_back(
      MakeAdWithUrl(KURL(String::FromUTF8("https://origin.test/"))));
  blink_interest_group->additional_bid_key = {
      0x7d, 0x4d, 0x0e, 0x7f, 0x61, 0x53, 0xa6, 0x9b, 0x62, 0x42, 0xb5,
      0x22, 0xab, 0xbe, 0xe6, 0x85, 0xfd, 0xa4, 0x42, 0x0f, 0x88, 0x34,
      0xb1, 0x08, 0xc3, 0xbd, 0xae, 0x36, 0x9e, 0xf5, 0x49, 0xfa};

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String(),
      /*expected_error_field_value=*/String(),
      /*expected_error=*/
      "Interest groups that provide a value of additionalBidKey for negative "
      "targeting must not provide a value for ads.");
}

TEST_F(ValidateBlinkInterestGroupTest, AggregationCoordinatorNotHTTPS) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->aggregation_coordinator_origin =
      SecurityOrigin::CreateFromString("http://coordinator.test");

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("aggregationCoordinatorOrigin"),
      /*expected_error_field_value=*/
      String::FromUTF8("http://coordinator.test"),
      /*expected_error=*/
      String::FromUTF8("aggregationCoordinatorOrigin origin must be HTTPS."));
}

TEST_F(ValidateBlinkInterestGroupTest, AggregationCoordinatorInvalid) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->aggregation_coordinator_origin =
      SecurityOrigin::CreateFromString("http://invalid^&");

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("aggregationCoordinatorOrigin"),
      /*expected_error_field_value=*/String::FromUTF8("null"),
      /*expected_error=*/
      String::FromUTF8("aggregationCoordinatorOrigin origin must be HTTPS."));
}

TEST_F(ValidateBlinkInterestGroupTest,
       AdditionalBidKeyAndUpdateURLNotAllowedTogether) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->update_url =
      KURL(String::FromUTF8("https://origin.test/update"));
  blink_interest_group->additional_bid_key = {
      0x7d, 0x4d, 0x0e, 0x7f, 0x61, 0x53, 0xa6, 0x9b, 0x62, 0x42, 0xb5,
      0x22, 0xab, 0xbe, 0xe6, 0x85, 0xfd, 0xa4, 0x42, 0x0f, 0x88, 0x34,
      0xb1, 0x08, 0xc3, 0xbd, 0xae, 0x36, 0x9e, 0xf5, 0x49, 0xfa};

  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/String(),
      /*expected_error_field_value=*/String(),
      /*expected_error=*/
      "Interest groups that provide a value of additionalBidKey for negative "
      "targeting must not provide an updateURL.");
}

TEST_F(ValidateBlinkInterestGroupTest,
       MaxTrustedBiddingSignalsURLLengthMustNotBeNegative) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->max_trusted_bidding_signals_url_length = -1;

  ExpectInterestGroupIsNotValid(
      blink_interest_group, /*expected_error_field_name=*/
      String::FromUTF8("maxTrustedBiddingSignalsURLLength"),
      /*expected_error_field_value=*/String::FromUTF8("-1"),
      /*expected_error=*/
      String::FromUTF8("maxTrustedBiddingSignalsURLLength is negative."));
}

TEST_F(ValidateBlinkInterestGroupTest,
       InvalidTrustedBiddingSignalsCoordinator) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->trusted_bidding_signals_coordinator =
      SecurityOrigin::CreateFromString(String::FromUTF8("http://origin.test/"));
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("trustedBiddingSignalsCoordinator"),
      /*expected_error_field_value=*/String::FromUTF8("http://origin.test"),
      /*expected_error=*/
      String::FromUTF8(
          "trustedBiddingSignalsCoordinator origin must be HTTPS."));

  blink_interest_group->trusted_bidding_signals_coordinator =
      SecurityOrigin::CreateFromString(String::FromUTF8("data:,foo"));
  // Data URLs have opaque origins, which are mapped to the string "null".
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      /*expected_error_field_name=*/
      String::FromUTF8("trustedBiddingSignalsCoordinator"),
      /*expected_error_field_value=*/String::FromUTF8("null"),
      /*expected_error=*/
      String::FromUTF8(
          "trustedBiddingSignalsCoordinator origin must be HTTPS."));
}

}  // namespace blink
