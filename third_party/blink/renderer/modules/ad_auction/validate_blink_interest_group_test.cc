// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

constexpr char kOriginString[] = "https://origin.test/";
constexpr char kNameString[] = "name";

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

  // Check that `blink_interest_group` is valid, if added from `blink_origin`,
  // and returns the provided error values.
  void ExpectInterestGroupIsNotValid(
      const mojom::blink::InterestGroupPtr& blink_interest_group,
      const std::string& expected_error_field_name,
      const std::string& expected_error_field_value,
      const std::string& expected_error) {
    String error_field_name;
    String error_field_value;
    String error;
    EXPECT_FALSE(ValidateBlinkInterestGroup(
        *blink_interest_group, error_field_name, error_field_value, error));
    EXPECT_EQ(String::FromUTF8(expected_error_field_name), error_field_name);
    EXPECT_EQ(String::FromUTF8(expected_error_field_value), error_field_value);
    EXPECT_EQ(String::FromUTF8(expected_error), error);

    blink::InterestGroup interest_group;
    // mojo deserialization will call InterestGroup::IsValid.
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<mojom::blink::InterestGroup>(
            blink_interest_group, interest_group));
  }

  // Creates and returns a minimally populated mojom::blink::InterestGroup.
  mojom::blink::InterestGroupPtr CreateMinimalInterestGroup() {
    mojom::blink::InterestGroupPtr blink_interest_group =
        mojom::blink::InterestGroup::New();
    blink_interest_group->owner = kOrigin;
    blink_interest_group->name = kName;
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
    blink_interest_group->daily_update_url = kAllowedUrl;
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
    blink_interest_group->user_bidding_signals =
        String::FromUTF8("\"This field isn't actually validated\"");

    // Add two ads. Use different URLs, with references.
    blink_interest_group->ads.emplace();
    auto mojo_ad1 = mojom::blink::InterestGroupAd::New();
    mojo_ad1->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?bar#baz"));
    mojo_ad1->metadata =
        String::FromUTF8("\"This field isn't actually validated\"");
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
    blink_interest_group->ad_components->push_back(
        std::move(mojo_ad_component1));
    auto mojo_ad_component2 = mojom::blink::InterestGroupAd::New();
    mojo_ad_component2->render_url =
        KURL(String::FromUTF8("https://origin.test/foo?component#baz2"));
    blink_interest_group->ad_components->push_back(
        std::move(mojo_ad_component2));

    return blink_interest_group;
  }

 protected:
  // SecurityOrigin used as the owner in most tests.
  const scoped_refptr<const SecurityOrigin> kOrigin =
      SecurityOrigin::CreateFromString(String::FromUTF8(kOriginString));

  const String kName = String::FromUTF8(kNameString);
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
      blink_interest_group, "owner" /* expected_error_field_name */,
      "http://origin.test" /* expected_error_field_value */,
      "owner origin must be HTTPS." /* expected_error */);

  blink_interest_group->owner =
      SecurityOrigin::CreateFromString(String::FromUTF8("data:,foo"));
  // Data URLs have opaque origins, which are mapped to the string "null".
  ExpectInterestGroupIsNotValid(
      blink_interest_group, "owner" /* expected_error_field_name */,
      "null" /* expected_error_field_value */,
      "owner origin must be HTTPS." /* expected_error */);
}

// Check that `bidding_url`, `bidding_wasm_helper_url`, `daily_update_url`, and
// `trusted_bidding_signals_url` must be same-origin and HTTPS.
//
// Ad URLs do not have to be same origin, so they're checked in a different
// test.
TEST_F(ValidateBlinkInterestGroupTest, RejectedUrls) {
  // Strings when each field has a bad URL, copied from cc file.
  const char kBadBiddingUrlError[] =
      "biddingUrl must have the same origin as the InterestGroup owner "
      "and have no fragment identifier or embedded credentials.";
  const char kBadBiddingWasmHelperUrlError[] =
      "biddingWasmHelperUrl must have the same origin as the InterestGroup "
      "owner and have no fragment identifier or embedded credentials.";
  const char kBadUpdateUrlError[] =
      "updateUrl must have the same origin as the InterestGroup owner "
      "and have no fragment identifier or embedded credentials.";
  const char kBadTrustedBiddingSignalsUrlError[] =
      "trustedBiddingSignalsUrl must have the same origin as the "
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

      // Invalid URLs.
      KURL(String::FromUTF8("")),
      KURL(String::FromUTF8("invalid url")),
      KURL(String::FromUTF8("https://!@#$%^&*()/")),
      KURL(String::FromUTF8("https://[1::::::2]/")),
      KURL(String::FromUTF8("https://origin.test/%00")),
  };

  for (const KURL& rejected_url : kRejectedUrls) {
    SCOPED_TRACE(rejected_url.GetString());

    // Test `bidding_url`.
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->bidding_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, "biddingUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        kBadBiddingUrlError /* expected_error */);

    // Test `bidding_wasm_helper_url`
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->bidding_wasm_helper_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        "biddingWasmHelperUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        kBadBiddingWasmHelperUrlError /* expected_error */);

    // Test `daily_update_url`.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->daily_update_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, "updateUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        // expected_error
        kBadUpdateUrlError /* expected_error */);

    // Test `trusted_bidding_signals_url`.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->trusted_bidding_signals_url = rejected_url;
    ExpectInterestGroupIsNotValid(
        blink_interest_group,
        "trustedBiddingSignalsUrl" /* expected_error_field_name */,
        rejected_url.GetString().Utf8() /* expected_error_field_value */,
        kBadTrustedBiddingSignalsUrlError /* expected_error */);
  }

  // `trusted_bidding_signals_url` also can't include query strings.
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  KURL rejected_url = KURL(String::FromUTF8("https://origin.test/?query"));
  blink_interest_group->trusted_bidding_signals_url = rejected_url;
  ExpectInterestGroupIsNotValid(
      blink_interest_group,
      "trustedBiddingSignalsUrl" /* expected_error_field_name */,
      rejected_url.GetString().Utf8() /* expected_error_field_value */,
      kBadTrustedBiddingSignalsUrlError /* expected_error */);
}

// Tests valid and invalid ad render URLs.
TEST_F(ValidateBlinkInterestGroupTest, AdRenderUrlValidation) {
  const char kBadAdUrlError[] =
      "renderUrls must be HTTPS and have no embedded credentials.";

  const struct {
    bool expect_allowed;
    const char* url;
  } kTestCases[] = {
      // Same origin URLs are allowed.
      {true, "https://origin.test/foo?bar"},

      // Cross origin URLs are allowed, as long as they're HTTPS.
      {true, "https://b.test/"},
      {true, "https://a.test:1234/"},

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
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        test_case_url, String() /* metadata */));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          "ad[0].renderUrl" /* expected_error_field_name */,
          test_case_url.GetString().Utf8() /* expected_error_field_value */,
          kBadAdUrlError /* expected_error */);
    }

    // Add an InterestGroup with the test cases's URL as the second ad's URL.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->ads.emplace();
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        KURL(String::FromUTF8("https://origin.test/")),
        String() /* metadata */));
    blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
        test_case_url, String() /* metadata */));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          "ad[1].renderUrl" /* expected_error_field_name */,
          test_case_url.GetString().Utf8() /* expected_error_field_value */,
          kBadAdUrlError /* expected_error */);
    }
  }
}

// Tests valid and invalid ad render URLs.
TEST_F(ValidateBlinkInterestGroupTest, AdComponentRenderUrlValidation) {
  const char kBadAdUrlError[] =
      "renderUrls must be HTTPS and have no embedded credentials.";

  const struct {
    bool expect_allowed;
    const char* url;
  } kTestCases[] = {
      // Same origin URLs are allowed.
      {true, "https://origin.test/foo?bar"},

      // Cross origin URLs are allowed, as long as they're HTTPS.
      {true, "https://b.test/"},
      {true, "https://a.test:1234/"},

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
        mojom::blink::InterestGroupAd::New(test_case_url,
                                           String() /* metadata */));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          "adComponent[0].renderUrl" /* expected_error_field_name */,
          test_case_url.GetString().Utf8() /* expected_error_field_value */,
          kBadAdUrlError /* expected_error */);
    }

    // Add an InterestGroup with the test cases's URL as the second ad
    // component's URL.
    blink_interest_group = CreateMinimalInterestGroup();
    blink_interest_group->ad_components.emplace();
    blink_interest_group->ad_components->emplace_back(
        mojom::blink::InterestGroupAd::New(
            KURL(String::FromUTF8("https://origin.test/")),
            String() /* metadata */));
    blink_interest_group->ad_components->emplace_back(
        mojom::blink::InterestGroupAd::New(test_case_url,
                                           String() /* metadata */));
    if (test_case.expect_allowed) {
      ExpectInterestGroupIsValid(blink_interest_group);
    } else {
      ExpectInterestGroupIsNotValid(
          blink_interest_group,
          "adComponent[1].renderUrl" /* expected_error_field_name */,
          test_case_url.GetString().Utf8() /* expected_error_field_value */,
          kBadAdUrlError /* expected_error */);
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
      "renderUrls must be HTTPS and have no embedded credentials.";
  mojom::blink::InterestGroupPtr blink_interest_group =
      mojom::blink::InterestGroup::New();
  blink_interest_group->owner = kOrigin;
  blink_interest_group->name = kName;
  blink_interest_group->ads.emplace();
  blink_interest_group->ads->emplace_back(mojom::blink::InterestGroupAd::New(
      KURL(kMalformedUrl), String() /* metadata */));
  String error_field_name;
  String error_field_value;
  String error;
  EXPECT_FALSE(ValidateBlinkInterestGroup(
      *blink_interest_group, error_field_name, error_field_value, error));
  EXPECT_EQ(error_field_name, String::FromUTF8("ad[0].renderUrl"));
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
  std::string long_string(51169, 'n');
  blink_interest_group->name = String(long_string);
  ExpectInterestGroupIsNotValid(
      blink_interest_group, "size" /* expected_error_field_name */,
      "51200" /* expected_error_field_value */,
      "interest groups must be less than 51200 bytes" /* expected_error */);

  // Almost too big should still work.
  long_string = std::string(51200 - 32, 'n');
  blink_interest_group->name = String(long_string);

  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, TooLargeAds) {
  mojom::blink::InterestGroupPtr blink_interest_group =
      CreateMinimalInterestGroup();
  blink_interest_group->name = "padding to 51200...";
  blink_interest_group->ad_components.emplace();
  for (int i = 0; i < 682; ++i) {
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
      blink_interest_group, "size" /* expected_error_field_name */,
      "51200" /* expected_error_field_value */,
      "interest groups must be less than 51200 bytes" /* expected_error */);

  // Almost too big should still work.
  blink_interest_group->ad_components->resize(681);

  ExpectInterestGroupIsValid(blink_interest_group);
}

TEST_F(ValidateBlinkInterestGroupTest, InvalidPriority) {
  struct {
    double priority;
    const char* priority_text;
  } test_cases[] = {
      {std::numeric_limits<double>::quiet_NaN(), "NaN"},
      {std::numeric_limits<double>::signaling_NaN(), "NaN"},
      {std::numeric_limits<double>::infinity(), "Infinity"},
      {-std::numeric_limits<double>::infinity(), "-Infinity"},
  };
  for (const auto& test_case : test_cases) {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->priority = test_case.priority;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, "priority" /* expected_error_field_name */,
        test_case.priority_text, /*expected_error_field_value */
        "priority must be finite." /* expected_error */);
  }
}

TEST_F(ValidateBlinkInterestGroupTest, InvalidExecutionMode) {
  struct {
    blink::InterestGroup::ExecutionMode execution_mode;
    const char* execution_mode_text;
  } test_cases[] = {
      {blink::InterestGroup::ExecutionMode::kFrozenContext, "2"},
      {static_cast<blink::InterestGroup::ExecutionMode>(-1), "-1"},
  };
  for (const auto& test_case : test_cases) {
    mojom::blink::InterestGroupPtr blink_interest_group =
        CreateMinimalInterestGroup();
    blink_interest_group->execution_mode = test_case.execution_mode;
    ExpectInterestGroupIsNotValid(
        blink_interest_group, "executionMode" /* expected_error_field_name */,
        test_case.execution_mode_text, /*expected_error_field_value */
        "execution mode is not valid." /* expected_error */);
  }
}

}  // namespace blink
