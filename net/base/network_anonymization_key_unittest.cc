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

struct EnabledFeatureFlagsTestingParam {
  const bool enableDoubleKeyNetworkAnonymizationKeyEnabled;
  const bool enableCrossSiteFlagNetworkAnonymizationKey;
  const bool enableDoubleKeyNetworkIsolationKey;
};

//    0. Triple-keying is enabled for both IsolationInfo and
//    NetworkAnonymizationKey.
//    1. Double-keying is enabled for both IsolationInfo and
//    NetworkAnonymizationKey.
//    2. Triple-keying is enabled for IsolationInfo and double-keying is enabled
//    for NetworkAnonymizationKey.
//    3. Triple-keying is enabled for IsolationInfo and double-keying +
//    cross-site-bit is enabled for NetworkAnonymizationKey.
const EnabledFeatureFlagsTestingParam kFlagsParam[] = {
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/false,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/false,
     /*enableDoubleKeyNetworkIsolationKey=*/false},
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/true,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/false,
     /*enableDoubleKeyNetworkIsolationKey=*/true},
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/true,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/false,
     /*enableDoubleKeyNetworkIsolationKey=*/false},
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/false,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/true,
     /*enableDoubleKeyNetworkIsolationKey=*/false}};

class NetworkAnonymizationKeyTest
    : public testing::Test,
      public testing::WithParamInterface<EnabledFeatureFlagsTestingParam> {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};

    if (IsDoubleKeyNetworkIsolationKeyEnabled()) {
      enabled_features.push_back(
          net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
    } else {
      disabled_features.push_back(
          net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
    }

    if (IsDoubleKeyNetworkAnonymizationKeyEnabled()) {
      enabled_features.push_back(
          net::features::kEnableDoubleKeyNetworkAnonymizationKey);
    } else {
      disabled_features.push_back(
          net::features::kEnableDoubleKeyNetworkAnonymizationKey);
    }

    if (IsCrossSiteFlagEnabled()) {
      enabled_features.push_back(
          net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
    } else {
      disabled_features.push_back(
          net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  static bool IsDoubleKeyNetworkIsolationKeyEnabled() {
    return GetParam().enableDoubleKeyNetworkIsolationKey;
  }

  bool IsDoubleKeyNetworkAnonymizationKeyEnabled() {
    return GetParam().enableDoubleKeyNetworkAnonymizationKeyEnabled;
  }

  bool IsCrossSiteFlagEnabled() {
    return GetParam().enableCrossSiteFlagNetworkAnonymizationKey;
  }

 protected:
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  const SchemefulSite kDataSite = SchemefulSite(GURL("data:foo"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkAnonymizationKeyTest,
    /*kEnableDoubleKeyNetworkAnonymizationKey*/ testing::ValuesIn(kFlagsParam));

TEST_P(NetworkAnonymizationKeyTest, IsDoubleKeyingEnabled) {
  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_FALSE(NetworkAnonymizationKey::IsFrameSiteEnabled());
  } else {
    EXPECT_TRUE(NetworkAnonymizationKey::IsFrameSiteEnabled());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsDoubleKeySchemeEnabled) {
  // Double key scheme is enabled only when
  // `kEnableDoubleKeyNetworkAnonymizationKeyEnabled` is enabled but
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is not.
  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() &&
      !IsCrossSiteFlagEnabled()) {
    EXPECT_TRUE(NetworkAnonymizationKey::IsDoubleKeySchemeEnabled());
  } else {
    EXPECT_FALSE(NetworkAnonymizationKey::IsDoubleKeySchemeEnabled());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsCrossSiteFlagSchemeEnabled) {
  // Double key with cross site flag scheme is enabled whenever
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled
  // regardless of the value of
  // `kEnableDoubleKeyNetworkAnonymizationKeyEnabled`.
  if (IsCrossSiteFlagEnabled()) {
    EXPECT_TRUE(NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled());
  } else {
    EXPECT_FALSE(NetworkAnonymizationKey::IsCrossSiteFlagSchemeEnabled());
  }
}

TEST_P(NetworkAnonymizationKeyTest, CreateFromNetworkIsolationKey) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.test/"));
  base::UnguessableToken nik_nonce = base::UnguessableToken::Create();
  NetworkIsolationKey populated_cross_site_nik(site_a, site_b, &nik_nonce);
  NetworkIsolationKey populated_same_site_nik(site_a, site_a, &nik_nonce);
  NetworkIsolationKey empty_nik;

  NetworkAnonymizationKey nak_from_cross_site_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_cross_site_nik);
  NetworkAnonymizationKey nak_from_same_site_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_same_site_nik);
  NetworkAnonymizationKey nak_from_empty_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(empty_nik);

  // NAKs created when there is no top frame site on the NIK should create an
  // empty NAK.
  EXPECT_TRUE(nak_from_empty_nik.IsEmpty());

  // Double-keyed NetworkIsolationKey + double-keyed NetworkAnonymizationKey
  // case.
  if (IsDoubleKeyNetworkIsolationKeyEnabled() &&
      IsDoubleKeyNetworkAnonymizationKeyEnabled()) {
    // Top site should be populated.
    EXPECT_EQ(nak_from_cross_site_nik.GetTopFrameSite(), site_a);
    EXPECT_EQ(nak_from_same_site_nik.GetTopFrameSite(), site_a);

    // Nonce should be populated.
    EXPECT_EQ(nak_from_same_site_nik.GetNonce(), nik_nonce);
    EXPECT_EQ(nak_from_cross_site_nik.GetNonce(), nik_nonce);

    // Frame site getter should CHECK.
    EXPECT_DEATH_IF_SUPPORTED(nak_from_cross_site_nik.GetFrameSite(), "");
    EXPECT_DEATH_IF_SUPPORTED(nak_from_same_site_nik.GetFrameSite(), "");

    // Double-keyed NAKs created from different third party cross site contexts
    // should be equal.
    EXPECT_TRUE(nak_from_same_site_nik == nak_from_cross_site_nik);
  }

  // Triple-keyed NetworkIsolationKey + triple-keyed NetworkAnonymizationKey
  // case.
  if (!IsDoubleKeyNetworkIsolationKeyEnabled() &&
      !IsDoubleKeyNetworkAnonymizationKeyEnabled() &&
      !IsCrossSiteFlagEnabled()) {
    // Top site should be populated correctly.
    EXPECT_EQ(nak_from_cross_site_nik.GetTopFrameSite(), site_a);
    EXPECT_EQ(nak_from_same_site_nik.GetTopFrameSite(), site_a);

    // Nonce should be populated correctly.
    EXPECT_EQ(nak_from_same_site_nik.GetNonce(), nik_nonce);
    EXPECT_EQ(nak_from_cross_site_nik.GetNonce(), nik_nonce);

    // Frame site getter should be populated correctly.
    EXPECT_EQ(nak_from_cross_site_nik.GetFrameSite(), site_b);
    EXPECT_EQ(nak_from_same_site_nik.GetFrameSite(), site_a);

    // Triple-keyed NAKs created from different third party cross site contexts
    // should be different.
    EXPECT_FALSE(nak_from_same_site_nik == nak_from_cross_site_nik);
  }

  // Triple-keyed NetworkIsolationKey + double-keyed NetworkAnonymizationKey
  // case.
  if (!IsDoubleKeyNetworkIsolationKeyEnabled() &&
      IsDoubleKeyNetworkAnonymizationKeyEnabled() &&
      !IsCrossSiteFlagEnabled()) {
    // Top site should be populated correctly.
    EXPECT_EQ(nak_from_cross_site_nik.GetTopFrameSite(), site_a);
    EXPECT_EQ(nak_from_same_site_nik.GetTopFrameSite(), site_a);

    // Nonce should be populated correctly.
    EXPECT_EQ(nak_from_same_site_nik.GetNonce(), nik_nonce);
    EXPECT_EQ(nak_from_cross_site_nik.GetNonce(), nik_nonce);

    // Frame site getter should not be accessible when the double keying is
    // enabled.
    EXPECT_DEATH_IF_SUPPORTED(nak_from_cross_site_nik.GetFrameSite(), "");
    EXPECT_DEATH_IF_SUPPORTED(nak_from_same_site_nik.GetFrameSite(), "");

    // Double-keyed NAKs created from different third party cross site contexts
    // should be the same.
    EXPECT_TRUE(nak_from_same_site_nik == nak_from_cross_site_nik);
  }

  // Triple-keyed NetworkIsolationKey + double-keyed + cross site bit
  // NetworkAnonymizationKey case.
  if (!IsDoubleKeyNetworkIsolationKeyEnabled() &&
      !IsDoubleKeyNetworkAnonymizationKeyEnabled() &&
      IsCrossSiteFlagEnabled()) {
    // Top site should be populated correctly.
    EXPECT_EQ(nak_from_cross_site_nik.GetTopFrameSite(), site_a);
    EXPECT_EQ(nak_from_same_site_nik.GetTopFrameSite(), site_a);

    // Nonce should be populated correctly.
    EXPECT_EQ(nak_from_same_site_nik.GetNonce(), nik_nonce);
    EXPECT_EQ(nak_from_cross_site_nik.GetNonce(), nik_nonce);

    // Frame site getter should not be accessible when the double keying is
    // enabled.
    EXPECT_DEATH_IF_SUPPORTED(nak_from_cross_site_nik.GetFrameSite(), "");
    EXPECT_DEATH_IF_SUPPORTED(nak_from_same_site_nik.GetFrameSite(), "");

    // Is cross site boolean should be populated correctly.
    EXPECT_EQ(nak_from_same_site_nik.GetIsCrossSite(), false);
    EXPECT_EQ(nak_from_cross_site_nik.GetIsCrossSite(), true);

    // Double-keyed + cross site bit NAKs created from different third party
    // cross site contexts should be the different.
    EXPECT_FALSE(nak_from_same_site_nik == nak_from_cross_site_nik);
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsEmpty) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsEmpty());
  EXPECT_FALSE(populated_key.IsEmpty());
}

TEST_P(NetworkAnonymizationKeyTest, CreateTransient) {
  NetworkAnonymizationKey transient_key1 =
      NetworkAnonymizationKey::CreateTransient();
  NetworkAnonymizationKey transient_key2 =
      NetworkAnonymizationKey::CreateTransient();

  EXPECT_TRUE(transient_key1.IsTransient());
  EXPECT_TRUE(transient_key2.IsTransient());
  EXPECT_FALSE(transient_key1 == transient_key2);
}

TEST_P(NetworkAnonymizationKeyTest, IsTransient) {
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

  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() || IsCrossSiteFlagEnabled()) {
    NetworkAnonymizationKey populated_double_key(/*top_frame_site=*/kTestSiteA,
                                                 /*frame_site=*/absl::nullopt,
                                                 /*is_cross_site=*/false,
                                                 /*nonce=*/absl::nullopt);
    EXPECT_FALSE(data_frame_key.IsTransient());
    EXPECT_FALSE(populated_double_key.IsTransient());
  } else {
    EXPECT_TRUE(data_frame_key.IsTransient());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsFullyPopulated) {
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
  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() || IsCrossSiteFlagEnabled()) {
    NetworkAnonymizationKey empty_frame_site_key(/*top_frame_site=*/kTestSiteA,
                                                 /*frame_site=*/absl::nullopt,
                                                 /*is_cross_site=*/false,
                                                 /*nonce=*/absl::nullopt);
    EXPECT_TRUE(empty_frame_site_key.IsFullyPopulated());
  }

  // is_cross_site is required when
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
  // Since we have both the top_frame_site and frame_site values the constructor
  // should calculate and set `is_cross_site`.
  EXPECT_TRUE(empty_cross_site_flag_key.IsFullyPopulated());
}

TEST_P(NetworkAnonymizationKeyTest, IsCrossSiteFlagCalculatedInConstructor) {
  if (IsCrossSiteFlagEnabled()) {
    NetworkAnonymizationKey cross_site_key(/*top_frame_site=*/kTestSiteA,
                                           /*frame_site=*/kTestSiteB,
                                           /*is_cross_site=*/true);
    NetworkAnonymizationKey equal_cross_site_key(/*top_frame_site=*/kTestSiteA,
                                                 /*frame_site=*/kTestSiteB);

    NetworkAnonymizationKey same_site_key(/*top_frame_site=*/kTestSiteA,
                                          /*frame_site=*/kTestSiteA,
                                          /*is_cross_site=*/false);
    NetworkAnonymizationKey equal_same_site_key(/*top_frame_site=*/kTestSiteA,
                                                /*frame_site=*/kTestSiteA);

    NetworkAnonymizationKey double_key_cross_site(/*top_frame_site=*/kTestSiteA,
                                                  /*frame_site=*/absl::nullopt,
                                                  true);
    EXPECT_EQ(cross_site_key.GetIsCrossSite().value(), true);
    EXPECT_EQ(equal_cross_site_key.GetIsCrossSite().value(), true);
    EXPECT_EQ(cross_site_key, equal_cross_site_key);

    EXPECT_EQ(same_site_key.GetIsCrossSite().value(), false);
    EXPECT_EQ(equal_same_site_key.GetIsCrossSite().value(), false);
    EXPECT_EQ(same_site_key, equal_same_site_key);

    EXPECT_EQ(double_key_cross_site.GetIsCrossSite().value(), true);
  }
}

TEST_P(NetworkAnonymizationKeyTest, Getters) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);

  EXPECT_EQ(key.GetTopFrameSite(), kTestSiteA);
  EXPECT_EQ(key.GetNonce(), kNonce);

  // frame_site should be empty if any double key scheme is enabled. This
  // includes when `kEnableCrossSiteFlagNetworkAnonymizationKey` or
  // `kEnableDoubleKeyNetworkAnonymizationKey` are enabled.
  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_DEATH_IF_SUPPORTED(key.GetFrameSite(), "");
    EXPECT_EQ(key.GetFrameSiteForTesting(), absl::nullopt);
  } else {
    EXPECT_EQ(key.GetFrameSite(), kTestSiteB);
  }

  // is_cross_site should only be true when
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
  if (IsCrossSiteFlagEnabled()) {
    EXPECT_TRUE(key.GetIsCrossSite());
  }
}

TEST_P(NetworkAnonymizationKeyTest, ToDebugString) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);
  NetworkAnonymizationKey empty_key;

  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() &&
      !IsCrossSiteFlagEnabled()) {
    // When double key scheme is enabled, the `is_cross_site` flag is always
    // forced to false.
    std::string double_key_expected_string_value = kTestSiteA.GetDebugString() +
                                                   " null" + " (with nonce " +
                                                   kNonce.ToString() + ")";
    EXPECT_EQ(key.ToDebugString(), double_key_expected_string_value);
    EXPECT_EQ(empty_key.ToDebugString(), "null null");
  } else if (IsCrossSiteFlagEnabled()) {
    // When double key + cross site flag scheme is enabled frame site is null,
    // but `is_cross_site` holds the value the key is created with.
    std::string double_key_with_cross_site_flag_expected_string_value =
        kTestSiteA.GetDebugString() + " null" + " cross_site (with nonce " +
        kNonce.ToString() + ")";
    EXPECT_EQ(key.ToDebugString(),
              double_key_with_cross_site_flag_expected_string_value);
    // is_cross_site_ will be stored as nullopt when it's not populated even if
    // IsCrossSiteFlagEnabled is enabled.
    EXPECT_EQ(empty_key.ToDebugString(),
              "null null with empty is_cross_site value");
  } else {
    // When neither `kEnableDoubleKeyNetworkAnonymizationKey` or
    // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled,
    // the NAK is a triple key and the `is_cross_site` flag is always forced
    // to false.
    std::string key_expected_string_value =
        kTestSiteA.GetDebugString() + " " + kTestSiteB.GetDebugString() +
        " (with nonce " + kNonce.ToString() + ")";
    EXPECT_EQ(key.ToDebugString(), key_expected_string_value);
    EXPECT_EQ(empty_key.ToDebugString(), "null null");
  }
}

TEST_P(NetworkAnonymizationKeyTest, Equality) {
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

  // The `is_cross_site` flag only changes the NAK when
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
  if (IsCrossSiteFlagEnabled()) {
    EXPECT_FALSE(key == key_cross_site);
    EXPECT_TRUE(key != key_cross_site);
    EXPECT_TRUE(key < key_cross_site);
  } else {
    EXPECT_TRUE(key == key_cross_site);
    EXPECT_FALSE(key != key_cross_site);
    EXPECT_FALSE(key < key_cross_site);
  }

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

  if (IsDoubleKeyNetworkAnonymizationKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_TRUE(key == key_different_frame_site);
    EXPECT_FALSE(key != key_different_frame_site);
  } else {
    EXPECT_FALSE(key == key_different_frame_site);
    EXPECT_TRUE(key != key_different_frame_site);
  }
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

TEST_P(NetworkAnonymizationKeyTest, ValueRoundTripCrossSite) {
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

TEST_P(NetworkAnonymizationKeyTest, ValueRoundTripSameSite) {
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

TEST_P(NetworkAnonymizationKeyTest, ValueRoundTripOpaqueFrameSite) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key(/*top_frame_site=*/kTestSiteA,
                                       /*frame_site=*/kOpaqueSite,
                                       /*is_cross_site=*/false);
  base::Value value;
  if (!NetworkAnonymizationKey::IsFrameSiteEnabled()) {
    ASSERT_TRUE(original_key.ToValue(&value));
    NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
    EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
    EXPECT_EQ(original_key, from_value_key);
  } else {
    // If we expect a valid frame site we should fail to serialize the garbage
    // value.
    ASSERT_FALSE(original_key.ToValue(&value));
  }
}

TEST_P(NetworkAnonymizationKeyTest, DeserializeValueWIthGarbageFrameSite) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  base::Value::List invalid_value;
  invalid_value.Append("http://a.test/");
  invalid_value.Append("data:text/html,junk");

  // If we expect a valid frame site we should fail to deserialize the garbage
  // value.
  if (NetworkAnonymizationKey::IsFrameSiteEnabled()) {
    NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
    EXPECT_FALSE(NetworkAnonymizationKey::FromValue(
        base::Value(std::move(invalid_value)), &from_value_key));
  }
}

TEST_P(NetworkAnonymizationKeyTest, TransientValueRoundTrip) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key =
      NetworkAnonymizationKey::CreateTransient();
  base::Value value;
  ASSERT_FALSE(original_key.ToValue(&value));
}

TEST_P(NetworkAnonymizationKeyTest, EmptyValueRoundTrip) {
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

  // Turn double keying off.
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);

  // Create a triple key.
  NetworkAnonymizationKey original_triple_key(/*top_frame_site=*/kTestSiteA,
                                              /*frame_site=*/kTestSiteB);

  // Serialize key to value while triple keying is enabled.
  base::Value triple_key_value;
  ASSERT_TRUE(original_triple_key.ToValue(&triple_key_value));

  // Convert it back to a triple keyed NetworkAnonymizationKey.
  NetworkAnonymizationKey from_value_triple_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(triple_key_value,
                                                 &from_value_triple_key));
  EXPECT_EQ(original_triple_key, from_value_triple_key);

  // Turn double keying on.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);

  // Check that deserializing the triple keyed value fails.
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(triple_key_value,
                                                  &expected_failure_nak));

  // Create a double keyed NetworkAnonymizationKey.
  NetworkAnonymizationKey original_double_key(/*top_frame_site=*/kTestSiteA);
  // Serialize key to value while double keying is enabled.
  base::Value double_key_value;
  ASSERT_TRUE(original_double_key.ToValue(&double_key_value));

  // Convert it back to a double keyed NetworkAnonymizationKey.
  NetworkAnonymizationKey from_value_double_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(double_key_value,
                                                 &from_value_double_key));
  EXPECT_EQ(original_double_key, from_value_double_key);

  // Turn double keying + cross site flag on.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);

  // Check that deserializing the triple keyed value fails.
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(triple_key_value,
                                                  &expected_failure_nak));

  // Check that deserializing the double keyed value fails.
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(double_key_value,
                                                  &expected_failure_nak));

  // Create a cross site double key + cross site flag NetworkAnonymizationKey.
  NetworkAnonymizationKey original_cross_site_double_key(
      /*top_frame_site=*/kTestSiteA,
      /*frame_site=*/kTestSiteB, false);
  // Serialize key to value while double key + cross site flag is enabled.
  base::Value cross_site_double_key_value;
  ASSERT_TRUE(
      original_cross_site_double_key.ToValue(&cross_site_double_key_value));

  // Convert it back to a double keyed NetworkAnonymizationKey.
  NetworkAnonymizationKey from_value_cross_site_double_key =
      NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(
      cross_site_double_key_value, &from_value_cross_site_double_key));
  EXPECT_EQ(original_cross_site_double_key, from_value_cross_site_double_key);

  // Turn double keying on.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);

  // Check that deserializing the cross site double keyed value fails.
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(cross_site_double_key_value,
                                                  &expected_failure_nak));

  // Turn triple keying back on.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);

  // Check that deserializing the double keyed value fails.
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(double_key_value,
                                                  &expected_failure_nak));

  // Check that deserializing the cross site double keyed value fails.
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(cross_site_double_key_value,
                                                  &expected_failure_nak));
}

}  // namespace net
