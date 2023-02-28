// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_anonymization_key.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "network_anonymization_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

class NetworkAnonymizationKeyTest : public testing::Test {
 protected:
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  const SchemefulSite kDataSite = SchemefulSite(GURL("data:foo"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();
};

TEST_F(NetworkAnonymizationKeyTest, CreateFromNetworkIsolationKey) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.test/"));
  SchemefulSite opaque = SchemefulSite(url::Origin());
  base::UnguessableToken nik_nonce = base::UnguessableToken::Create();

  NetworkIsolationKey populated_cross_site_nik(site_a, site_b, &nik_nonce);
  NetworkIsolationKey populated_same_site_nik(site_a, site_a, &nik_nonce);
  NetworkIsolationKey populated_same_site_opaque_nik(opaque, opaque,
                                                     &nik_nonce);
  NetworkIsolationKey empty_nik;

  NetworkAnonymizationKey nak_from_same_site_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_same_site_nik);
  NetworkAnonymizationKey nak_from_cross_site_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_cross_site_nik);
  NetworkAnonymizationKey nak_from_same_site_opaque_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_same_site_opaque_nik);
  NetworkAnonymizationKey nak_from_empty_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(empty_nik);

  // NAKs created when there is no top frame site on the NIK should create an
  // empty NAK.
  EXPECT_TRUE(nak_from_empty_nik.IsEmpty());

  // Top site should be populated correctly.
  EXPECT_EQ(nak_from_same_site_nik.GetTopFrameSite(), site_a);
  EXPECT_EQ(nak_from_cross_site_nik.GetTopFrameSite(), site_a);
  EXPECT_EQ(nak_from_same_site_opaque_nik.GetTopFrameSite(), opaque);

  // Nonce should be populated correctly.
  EXPECT_EQ(nak_from_same_site_nik.GetNonce(), nik_nonce);
  EXPECT_EQ(nak_from_cross_site_nik.GetNonce(), nik_nonce);
  EXPECT_EQ(nak_from_same_site_opaque_nik.GetNonce(), nik_nonce);

  // Is cross site boolean should be populated correctly.
  EXPECT_EQ(nak_from_same_site_nik.GetIsCrossSite(), false);
  EXPECT_EQ(nak_from_cross_site_nik.GetIsCrossSite(), true);
  EXPECT_EQ(nak_from_same_site_opaque_nik.GetIsCrossSite(), false);

  // Double-keyed + cross site bit NAKs created from different third party
  // cross site contexts should be the different.
  EXPECT_FALSE(nak_from_same_site_nik == nak_from_cross_site_nik);
}

TEST_F(NetworkAnonymizationKeyTest, IsEmpty) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsEmpty());
  EXPECT_FALSE(populated_key.IsEmpty());
}

TEST_F(NetworkAnonymizationKeyTest, CreateTransient) {
  NetworkAnonymizationKey transient_key1 =
      NetworkAnonymizationKey::CreateTransient();
  NetworkAnonymizationKey transient_key2 =
      NetworkAnonymizationKey::CreateTransient();

  EXPECT_TRUE(transient_key1.IsTransient());
  EXPECT_TRUE(transient_key2.IsTransient());
  EXPECT_FALSE(transient_key1 == transient_key2);
}

TEST_F(NetworkAnonymizationKeyTest, IsTransient) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey data_top_frame_key(/*top_frame_site=*/kDataSite,
                                             /*frame_site=*/kTestSiteB,
                                             /*is_cross_site=*/false,
                                             /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey populated_key_with_nonce(
      /*top_frame_site=*/kTestSiteA, /*frame_site=*/kTestSiteB,
      /*is_cross_site*/ false, base::UnguessableToken::Create());
  NetworkAnonymizationKey data_frame_key(/*top_frame_site=*/kTestSiteA,
                                         /*frame_site=*/kDataSite,
                                         /*is_cross_site=*/false,
                                         /*nonce=*/absl::nullopt);

  NetworkAnonymizationKey from_create_transient =
      NetworkAnonymizationKey::CreateTransient();

  EXPECT_TRUE(empty_key.IsTransient());
  EXPECT_FALSE(populated_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(populated_key_with_nonce.IsTransient());
  EXPECT_TRUE(from_create_transient.IsTransient());

  NetworkAnonymizationKey populated_double_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/absl::nullopt,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);
  EXPECT_FALSE(data_frame_key.IsTransient());
  EXPECT_FALSE(populated_double_key.IsTransient());
}

TEST_F(NetworkAnonymizationKeyTest, IsFullyPopulated) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey empty_cross_site_flag_key(
      /*top_frame_site=*/kTestSiteA,
      /*frame_site=*/kTestSiteB,
      /*is_cross_site=*/absl::nullopt,
      /*nonce=*/absl::nullopt);
  EXPECT_TRUE(populated_key.IsFullyPopulated());
  EXPECT_FALSE(empty_key.IsFullyPopulated());
  NetworkAnonymizationKey empty_frame_site_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/absl::nullopt,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);
  EXPECT_TRUE(empty_frame_site_key.IsFullyPopulated());

  // is_cross_site is required. Since we have both the top_frame_site and
  // frame_site values the constructor should calculate and set `is_cross_site`.
  EXPECT_TRUE(empty_cross_site_flag_key.IsFullyPopulated());
}

TEST_F(NetworkAnonymizationKeyTest, IsCrossSiteFlagCalculatedInConstructor) {
  NetworkAnonymizationKey cross_site_key(/*top_frame_site=*/kTestSiteA,
                                         /*frame_site=*/kTestSiteB,
                                         /*is_cross_site=*/true);
  NetworkAnonymizationKey equal_cross_site_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/kTestSiteB);
  EXPECT_EQ(cross_site_key.GetIsCrossSite().value(), true);
  EXPECT_EQ(equal_cross_site_key.GetIsCrossSite().value(), true);
  EXPECT_EQ(cross_site_key, equal_cross_site_key);

  NetworkAnonymizationKey same_site_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteA,
                                        /*is_cross_site=*/false);
  NetworkAnonymizationKey equal_same_site_key(/*top_frame_site=*/kTestSiteA,
                                              /*frame_site=*/kTestSiteA);
  EXPECT_EQ(same_site_key.GetIsCrossSite().value(), false);
  EXPECT_EQ(equal_same_site_key.GetIsCrossSite().value(), false);
  EXPECT_EQ(same_site_key, equal_same_site_key);

  SchemefulSite opaque = SchemefulSite(url::Origin());
  NetworkAnonymizationKey same_opaque_site_key(/*top_frame_site=*/opaque,
                                               /*frame_site=*/opaque,
                                               /*is_cross_site=*/false);
  NetworkAnonymizationKey equal_same_opaque_site_key(
      /*top_frame_site=*/opaque,
      /*frame_site=*/opaque);
  EXPECT_EQ(same_opaque_site_key.GetIsCrossSite().value(), false);
  EXPECT_EQ(equal_same_opaque_site_key.GetIsCrossSite().value(), false);
  EXPECT_EQ(same_opaque_site_key, equal_same_opaque_site_key);

  SchemefulSite opaque2 = SchemefulSite(url::Origin());
  NetworkAnonymizationKey diff_opaque_site_key(/*top_frame_site=*/opaque,
                                               /*frame_site=*/opaque2,
                                               /*is_cross_site=*/true);
  NetworkAnonymizationKey equal_diff_opaque_site_key(
      /*top_frame_site=*/opaque,
      /*frame_site=*/opaque2);
  EXPECT_EQ(diff_opaque_site_key.GetIsCrossSite().value(), true);
  EXPECT_EQ(equal_diff_opaque_site_key.GetIsCrossSite().value(), true);
  EXPECT_EQ(diff_opaque_site_key, equal_diff_opaque_site_key);

  NetworkAnonymizationKey double_key_cross_site(/*top_frame_site=*/kTestSiteA,
                                                /*frame_site=*/absl::nullopt,
                                                true);
  EXPECT_EQ(double_key_cross_site.GetIsCrossSite().value(), true);
}

TEST_F(NetworkAnonymizationKeyTest, Getters) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);

  EXPECT_EQ(key.GetTopFrameSite(), kTestSiteA);
  EXPECT_EQ(key.GetNonce(), kNonce);

  EXPECT_TRUE(key.GetIsCrossSite().value());
}

TEST_F(NetworkAnonymizationKeyTest, ToDebugString) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);
  NetworkAnonymizationKey empty_key;

  // `is_cross_site` holds the value the key is created with.
  std::string double_key_with_cross_site_flag_expected_string_value =
      kTestSiteA.GetDebugString() + " cross_site (with nonce " +
      kNonce.ToString() + ")";
  EXPECT_EQ(key.ToDebugString(),
            double_key_with_cross_site_flag_expected_string_value);
  EXPECT_EQ(empty_key.ToDebugString(), "null");
}

TEST_F(NetworkAnonymizationKeyTest, Equality) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/false, kNonce);
  NetworkAnonymizationKey key_duplicate(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false, kNonce);
  EXPECT_TRUE(key == key_duplicate);
  EXPECT_FALSE(key != key_duplicate);
  EXPECT_FALSE(key < key_duplicate);

  NetworkAnonymizationKey key_cross_site(/*top_frame_site=*/kTestSiteA,
                                         /*frame_site=*/kTestSiteB,
                                         /*is_cross_site=*/true, kNonce);

  // The `is_cross_site` flag changes the NAK.
  EXPECT_FALSE(key == key_cross_site);
  EXPECT_TRUE(key != key_cross_site);
  EXPECT_TRUE(key < key_cross_site);

  NetworkAnonymizationKey key_no_nonce(/*top_frame_site=*/kTestSiteA,
                                       /*frame_site=*/kTestSiteB,
                                       /*is_cross_site=*/false,
                                       /*nonce=*/absl::nullopt);
  EXPECT_FALSE(key == key_no_nonce);
  EXPECT_TRUE(key != key_no_nonce);
  EXPECT_FALSE(key < key_no_nonce);

  NetworkAnonymizationKey key_different_nonce(
      /*top_frame_site=*/kTestSiteA,
      /*frame_site=*/kTestSiteB,
      /*is_cross_site=*/false,
      /*nonce=*/base::UnguessableToken::Create());
  EXPECT_FALSE(key == key_different_nonce);
  EXPECT_TRUE(key != key_different_nonce);

  NetworkAnonymizationKey key_different_frame_site(
      /*top_frame_site=*/kTestSiteA, /*frame_site=*/kTestSiteA,
      /*is_cross_site=*/false, kNonce);

  EXPECT_TRUE(key == key_different_frame_site);
  EXPECT_FALSE(key != key_different_frame_site);
  EXPECT_FALSE(key < key_different_frame_site);

  NetworkAnonymizationKey key_different_top_level_site(
      /*top_frame_site=*/kTestSiteB, /*frame_site=*/kTestSiteB,
      /*is_cross_site=*/false, kNonce);
  EXPECT_FALSE(key == key_different_top_level_site);
  EXPECT_TRUE(key != key_different_top_level_site);
  EXPECT_TRUE(key < key_different_top_level_site);

  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey empty_key_duplicate;
  EXPECT_TRUE(empty_key == empty_key_duplicate);
  EXPECT_FALSE(empty_key != empty_key_duplicate);
  EXPECT_FALSE(empty_key < empty_key_duplicate);

  EXPECT_FALSE(empty_key == key);
  EXPECT_TRUE(empty_key != key);
  EXPECT_TRUE(empty_key < key);
}

TEST_F(NetworkAnonymizationKeyTest, ValueRoundTripCrossSite) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key(/*top_frame_site=*/kTestSiteA,
                                       /*frame_site=*/kTestSiteB,
                                       /*is_cross_site=*/true);
  base::Value value;
  ASSERT_TRUE(original_key.ToValue(&value));

  // Fill initial value with opaque data, to make sure it's overwritten.
  NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
  EXPECT_EQ(original_key, from_value_key);
}

TEST_F(NetworkAnonymizationKeyTest, ValueRoundTripSameSite) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key(/*top_frame_site=*/kTestSiteA,
                                       /*frame_site=*/kTestSiteA,
                                       /*is_cross_site=*/false);
  base::Value value;
  ASSERT_TRUE(original_key.ToValue(&value));

  // Fill initial value with opaque data, to make sure it's overwritten.
  NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
  EXPECT_EQ(original_key, from_value_key);
}

TEST_F(NetworkAnonymizationKeyTest, TransientValueRoundTrip) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key =
      NetworkAnonymizationKey::CreateTransient();
  base::Value value;
  ASSERT_FALSE(original_key.ToValue(&value));
}

TEST_F(NetworkAnonymizationKeyTest, EmptyValueRoundTrip) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key;
  base::Value value;
  ASSERT_TRUE(original_key.ToValue(&value));

  // Fill initial value with opaque data, to make sure it's overwritten.
  NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
  EXPECT_EQ(original_key, from_value_key);
}

TEST(NetworkAnonymizationKeyFeatureShiftTest,
     ValueRoundTripKeySchemeMissmatch) {
  base::test::ScopedFeatureList scoped_feature_list_;
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  NetworkAnonymizationKey expected_failure_nak = NetworkAnonymizationKey();

  // Create a cross site double key + cross site flag NetworkAnonymizationKey.
  NetworkAnonymizationKey original_cross_site_double_key(
      /*top_frame_site=*/kTestSiteA,
      /*frame_site=*/kTestSiteB, false);
  base::Value cross_site_double_key_value;
  ASSERT_TRUE(
      original_cross_site_double_key.ToValue(&cross_site_double_key_value));

  // Check that deserializing a double keyed NetworkAnonymizationKey (a
  // one-element list) fails, using the serialized site from
  // `cross_site_double_key_value` to build it.
  base::Value serialized_site =
      cross_site_double_key_value.GetList()[0].Clone();
  base::Value::List double_key_list;
  double_key_list.Append(serialized_site.Clone());
  base::Value double_key_value = base::Value(std::move(double_key_list));
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(double_key_value,
                                                  &expected_failure_nak));

  // Check that deserializing a triple keyed value (a 2-element list containing
  // two sites) fails.
  base::Value::List triple_key_list;
  triple_key_list.Append(serialized_site.Clone());
  triple_key_list.Append(std::move(serialized_site));
  base::Value triple_key_value = base::Value(std::move(triple_key_list));
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(triple_key_value,
                                                  &expected_failure_nak));

  // Convert the successful value back to a NAK and verify.
  NetworkAnonymizationKey from_value_cross_site_double_key =
      NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(
      cross_site_double_key_value, &from_value_cross_site_double_key));
  EXPECT_EQ(original_cross_site_double_key, from_value_cross_site_double_key);
}

}  // namespace net
