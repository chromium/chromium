// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

#include "url/gurl.h"
#include "url/origin.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_B "https://example.net/"
#define ORIGIN_C "https://example.org/"

class GURL;

namespace blink {

namespace {

const char* const kValidPolicies[] = {
    "",      // An empty policy.
    " ",     // An empty policy.
    ";;",    // Empty policies.
    ",,",    // Empty policies.
    " ; ;",  // Empty policies.
    " , ,",  // Empty policies.
    ",;,",   // Empty policies.
    "geolocation 'none'",
    "geolocation 'self'",
    "geolocation 'src'",  // Only valid for iframe allow attribute.
    "geolocation",        // Only valid for iframe allow attribute.
    "geolocation; fullscreen; payment",
    "geolocation *",
    "geolocation " ORIGIN_A "",
    "geolocation " ORIGIN_B "",
    "geolocation  " ORIGIN_A " " ORIGIN_B "",
    "geolocation 'none' " ORIGIN_A " " ORIGIN_B "",
    "geolocation " ORIGIN_A " 'none' " ORIGIN_B "",
    "geolocation 'none' 'none' 'none'",
    "geolocation " ORIGIN_A " *",
    "fullscreen  " ORIGIN_A "; payment 'self'",
    "fullscreen " ORIGIN_A "; payment *, geolocation 'self'"};

const char* const kInvalidPolicies[] = {
    "badfeaturename",
    "badfeaturename 'self'",
    "1.0",
    "geolocation data://badorigin",
    "geolocation https://bad;origin",
    "geolocation https:/bad,origin",
    "geolocation https://example.com, https://a.com",
    "geolocation *, payment data://badorigin",
    "geolocation ws://xn--fd\xbcwsw3taaaaaBaa333aBBBBBBJBBJBBBt"};

}  // namespace

class FeaturePolicyParserTest : public testing::Test {
 protected:
  FeaturePolicyParserTest() = default;

  ~FeaturePolicyParserTest() override = default;

  /*void SetUp() override {
    chrome_client_ = new ConsoleCapturingChromeClient();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    SetupPageWithClients(&clients);
    Page::InsertOrdinaryPageForTesting(&GetPage());
  }*/

  scoped_refptr<const SecurityOrigin> origin_a_ =
      SecurityOrigin::CreateFromString(ORIGIN_A);
  scoped_refptr<const SecurityOrigin> origin_b_ =
      SecurityOrigin::CreateFromString(ORIGIN_B);
  scoped_refptr<const SecurityOrigin> origin_c_ =
      SecurityOrigin::CreateFromString(ORIGIN_C);

  url::Origin expected_url_origin_a_ = url::Origin::Create(GURL(ORIGIN_A));
  url::Origin expected_url_origin_b_ = url::Origin::Create(GURL(ORIGIN_B));
  url::Origin expected_url_origin_c_ = url::Origin::Create(GURL(ORIGIN_C));

  const FeatureNameMap test_feature_name_map = {
      {"fullscreen", blink::mojom::FeaturePolicyFeature::kFullscreen},
      {"payment", blink::mojom::FeaturePolicyFeature::kPayment},
      {"geolocation", blink::mojom::FeaturePolicyFeature::kGeolocation}};
};

TEST_F(FeaturePolicyParserTest, ParseValidPolicy) {
  Vector<String> messages;
  for (const char* policy_string : kValidPolicies) {
    messages.clear();
    ParseFeaturePolicy(policy_string, origin_a_.get(), origin_b_.get(),
                       &messages, test_feature_name_map);
    EXPECT_EQ(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyParserTest, ParseInvalidPolicy) {
  Vector<String> messages;
  for (const char* policy_string : kInvalidPolicies) {
    messages.clear();
    ParseFeaturePolicy(policy_string, origin_a_.get(), origin_b_.get(),
                       &messages, test_feature_name_map);
    EXPECT_NE(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyParserTest, PolicyParsedCorrectly) {
  Vector<String> messages;

  // Empty policy.
  ParsedFeaturePolicy parsed_policy = ParseFeaturePolicy(
      "", origin_a_.get(), origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(0UL, parsed_policy.size());

  // Simple policy with 'self'.
  parsed_policy =
      ParseFeaturePolicy("geolocation 'self'", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[0].origins.size());
  EXPECT_TRUE(
      parsed_policy[0].origins[0].IsSameOriginWith(expected_url_origin_a_));

  // Simple policy with *.
  parsed_policy =
      ParseFeaturePolicy("geolocation *", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_TRUE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());

  // Complicated policy.
  parsed_policy = ParseFeaturePolicy(
      "geolocation *; "
      "fullscreen https://example.net https://example.org; "
      "payment 'self'",
      origin_a_.get(), origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_TRUE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_FALSE(parsed_policy[1].matches_all_origins);
  EXPECT_FALSE(parsed_policy[1].matches_opaque_src);
  EXPECT_EQ(2UL, parsed_policy[1].origins.size());
  EXPECT_TRUE(
      parsed_policy[1].origins[0].IsSameOriginWith(expected_url_origin_b_));
  EXPECT_TRUE(
      parsed_policy[1].origins[1].IsSameOriginWith(expected_url_origin_c_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_FALSE(parsed_policy[2].matches_all_origins);
  EXPECT_FALSE(parsed_policy[2].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[2].origins.size());
  EXPECT_TRUE(
      parsed_policy[2].origins[0].IsSameOriginWith(expected_url_origin_a_));

  // Multiple policies.
  parsed_policy = ParseFeaturePolicy(
      "geolocation * https://example.net; "
      "fullscreen https://example.net none https://example.org,"
      "payment 'self' badorigin",
      origin_a_.get(), origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_TRUE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_FALSE(parsed_policy[1].matches_all_origins);
  EXPECT_FALSE(parsed_policy[1].matches_opaque_src);
  EXPECT_EQ(2UL, parsed_policy[1].origins.size());
  EXPECT_TRUE(
      parsed_policy[1].origins[0].IsSameOriginWith(expected_url_origin_b_));
  EXPECT_TRUE(
      parsed_policy[1].origins[1].IsSameOriginWith(expected_url_origin_c_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_FALSE(parsed_policy[2].matches_all_origins);
  EXPECT_FALSE(parsed_policy[2].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[2].origins.size());
  EXPECT_TRUE(
      parsed_policy[2].origins[0].IsSameOriginWith(expected_url_origin_a_));

  // Header policies with no optional origin lists.
  parsed_policy =
      ParseFeaturePolicy("geolocation;fullscreen;payment", origin_a_.get(),
                         nullptr, &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[0].origins.size());
  EXPECT_TRUE(
      parsed_policy[0].origins[0].IsSameOriginWith(expected_url_origin_a_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_FALSE(parsed_policy[1].matches_all_origins);
  EXPECT_FALSE(parsed_policy[1].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[1].origins.size());
  EXPECT_TRUE(
      parsed_policy[1].origins[0].IsSameOriginWith(expected_url_origin_a_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_FALSE(parsed_policy[2].matches_all_origins);
  EXPECT_FALSE(parsed_policy[2].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[2].origins.size());
  EXPECT_TRUE(
      parsed_policy[2].origins[0].IsSameOriginWith(expected_url_origin_a_));
}

TEST_F(FeaturePolicyParserTest, PolicyParsedCorrectlyForOpaqueOrigins) {
  Vector<String> messages;

  scoped_refptr<SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();

  // Empty policy.
  ParsedFeaturePolicy parsed_policy =
      ParseFeaturePolicy("", origin_a_.get(), opaque_origin.get(), &messages,
                         test_feature_name_map);
  EXPECT_EQ(0UL, parsed_policy.size());

  // Simple policy.
  parsed_policy =
      ParseFeaturePolicy("geolocation", origin_a_.get(), opaque_origin.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_TRUE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());

  // Simple policy with 'src'.
  parsed_policy =
      ParseFeaturePolicy("geolocation 'src'", origin_a_.get(),
                         opaque_origin.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_TRUE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());

  // Simple policy with *.
  parsed_policy =
      ParseFeaturePolicy("geolocation *", origin_a_.get(), opaque_origin.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_TRUE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());

  // Policy with explicit origins
  parsed_policy = ParseFeaturePolicy(
      "geolocation https://example.net https://example.org", origin_a_.get(),
      opaque_origin.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_FALSE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(2UL, parsed_policy[0].origins.size());
  EXPECT_TRUE(
      parsed_policy[0].origins[0].IsSameOriginWith(expected_url_origin_b_));
  EXPECT_TRUE(
      parsed_policy[0].origins[1].IsSameOriginWith(expected_url_origin_c_));

  // Policy with multiple origins, including 'src'.
  parsed_policy = ParseFeaturePolicy("geolocation https://example.net 'src'",
                                     origin_a_.get(), opaque_origin.get(),
                                     &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_TRUE(parsed_policy[0].matches_opaque_src);
  EXPECT_EQ(1UL, parsed_policy[0].origins.size());
  EXPECT_TRUE(
      parsed_policy[0].origins[0].IsSameOriginWith(expected_url_origin_b_));
}

// Test histogram counting the use of feature policies in header.
TEST_F(FeaturePolicyParserTest, HeaderHistogram) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  HistogramTester tester;
  Vector<String> messages;

  ParseFeaturePolicy("payment; fullscreen", origin_a_.get(), nullptr, &messages,
                     test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 1);
}

// Test counting the use of each feature policy only once per header.
TEST_F(FeaturePolicyParserTest, HistogramMultiple) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  HistogramTester tester;
  Vector<String> messages;

  // If the same feature is listed multiple times, it should only be counted
  // once.
  ParseFeaturePolicy("geolocation 'self'; payment; geolocation *",
                     origin_a_.get(), nullptr, &messages,
                     test_feature_name_map);
  ParseFeaturePolicy("fullscreen 'self', fullscreen *", origin_a_.get(),
                     nullptr, &messages, test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kGeolocation), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 1);
}

// Test histogram counting the use of feature policies via "allow"
// attribute. This test parses two policies on the same document.
TEST_F(FeaturePolicyParserTest, AllowHistogramSameDocument) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Allow";
  HistogramTester tester;
  Vector<String> messages;
  std::unique_ptr<DummyPageHolder> dummy = DummyPageHolder::Create();

  ParseFeaturePolicy("payment; fullscreen", origin_a_.get(), origin_b_.get(),
                     &messages, test_feature_name_map, &dummy->GetDocument());
  ParseFeaturePolicy("fullscreen; geolocation", origin_a_.get(),
                     origin_b_.get(), &messages, test_feature_name_map,
                     &dummy->GetDocument());
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kGeolocation), 1);
}

// Test histogram counting the use of feature policies via "allow"
// attribute. This test parses two policies on different documents.
TEST_F(FeaturePolicyParserTest, AllowHistogramDifferentDocument) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Allow";
  HistogramTester tester;
  Vector<String> messages;
  std::unique_ptr<DummyPageHolder> dummy = DummyPageHolder::Create();
  std::unique_ptr<DummyPageHolder> dummy2 = DummyPageHolder::Create();

  ParseFeaturePolicy("payment; fullscreen", origin_a_.get(), origin_b_.get(),
                     &messages, test_feature_name_map, &dummy->GetDocument());
  ParseFeaturePolicy("fullscreen; geolocation", origin_a_.get(),
                     origin_b_.get(), &messages, test_feature_name_map,
                     &dummy2->GetDocument());
  tester.ExpectTotalCount(histogram_name, 4);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kGeolocation), 1);
}

// Test policy mutation methods
class FeaturePolicyMutationTest : public testing::Test {
 protected:
  FeaturePolicyMutationTest() = default;

  ~FeaturePolicyMutationTest() override = default;

  url::Origin url_origin_a_ = url::Origin::Create(GURL(ORIGIN_A));
  url::Origin url_origin_b_ = url::Origin::Create(GURL(ORIGIN_B));
  url::Origin url_origin_c_ = url::Origin::Create(GURL(ORIGIN_C));

  // Returns true if the policy contains a declaration for the feature which
  // allows it in all origins.
  bool IsFeatureAllowedEverywhere(mojom::FeaturePolicyFeature feature,
                                  const ParsedFeaturePolicy& policy) {
    const auto& result = std::find_if(policy.begin(), policy.end(),
                                      [feature](const auto& declaration) {
                                        return declaration.feature == feature;
                                      });
    if (result == policy.end())
      return false;

    return result->feature == feature && result->matches_all_origins &&
           result->matches_opaque_src && result->origins.empty();
  }

  // Returns true if the policy contains a declaration for the feature which
  // disallows it in all origins.
  bool IsFeatureDisallowedEverywhere(mojom::FeaturePolicyFeature feature,
                                     const ParsedFeaturePolicy& policy) {
    const auto& result = std::find_if(policy.begin(), policy.end(),
                                      [feature](const auto& declaration) {
                                        return declaration.feature == feature;
                                      });
    if (result == policy.end())
      return false;

    return result->feature == feature && !result->matches_all_origins &&
           !result->matches_opaque_src && result->origins.empty();
  }

  ParsedFeaturePolicy test_policy = {{mojom::FeaturePolicyFeature::kFullscreen,
                                      false,
                                      false,
                                      {url_origin_a_, url_origin_b_}},
                                     {mojom::FeaturePolicyFeature::kGeolocation,
                                      false,
                                      false,
                                      {url_origin_a_}}};
  ParsedFeaturePolicy empty_policy = {};
};
TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclared) {
  EXPECT_TRUE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                test_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kUsb, test_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kNotFound, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclaredWithEmptyPolicy) {
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen,
                                 empty_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kNotFound, empty_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAbsentFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kPayment, test_policy));
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kPayment,
                                      test_policy));
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFromEmptyPolicy) {
  ASSERT_EQ(0UL, empty_policy.size());
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kPayment,
                                      test_policy));
  ASSERT_EQ(0UL, empty_policy.size());
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresent) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kFullscreen,
                                     test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kFullscreen,
                                      test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresentOnSecondFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kGeolocation,
                                     test_policy));
  ASSERT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                 test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kGeolocation,
                                      test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                 test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAllFeatures) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kFullscreen,
                                     test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kGeolocation,
                                     test_policy));
  EXPECT_EQ(0UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                 test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowIfNotPresent) {
  ParsedFeaturePolicy copy = test_policy;
  // Try to disallow a feature which already exists
  EXPECT_FALSE(DisallowFeatureIfNotPresent(
      mojom::FeaturePolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Disallow a new feature
  EXPECT_TRUE(
      DisallowFeatureIfNotPresent(mojom::FeaturePolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::FeaturePolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowEverywhereIfNotPresent) {
  ParsedFeaturePolicy copy = test_policy;
  // Try to allow a feature which already exists
  EXPECT_FALSE(AllowFeatureEverywhereIfNotPresent(
      mojom::FeaturePolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Allow a new feature
  EXPECT_TRUE(AllowFeatureEverywhereIfNotPresent(
      mojom::FeaturePolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, allowed everywhere
  EXPECT_TRUE(
      IsFeatureAllowedEverywhere(mojom::FeaturePolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowUnconditionally) {
  // Try to disallow a feature which already exists
  DisallowFeature(mojom::FeaturePolicyFeature::kFullscreen, test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowNewFeatureUnconditionally) {
  // Try to disallow a feature which does not yet exist
  DisallowFeature(mojom::FeaturePolicyFeature::kPayment, test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::FeaturePolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowUnconditionally) {
  // Try to allow a feature which already exists
  AllowFeatureEverywhere(mojom::FeaturePolicyFeature::kFullscreen, test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowNewFeatureUnconditionally) {
  // Try to allow a feature which does not yet exist
  AllowFeatureEverywhere(mojom::FeaturePolicyFeature::kPayment, test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(mojom::FeaturePolicyFeature::kPayment,
                                         test_policy));
}

}  // namespace blink
