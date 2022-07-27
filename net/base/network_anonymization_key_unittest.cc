// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_anonymization_key.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

struct EnabledFeatureFlagsTestingParam {
  bool enableDoubleKeyNetworkAnonymizationKeyEnabled;
  bool enableCrossSiteFlagNetworkAnonymizationKey;
};

const EnabledFeatureFlagsTestingParam kFlagsParam[] = {
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/false,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/false},
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/false,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/true},
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/true,
     /*enableCrossSiteFlagNetworkAnonymizationKey=*/false},
    {/*enableDoubleKeyNetworkAnonymizationKeyEnabled=*/true,
     /*enableCrossSiteFlagNetworkAnonymizationKe=y*/ true},
};

class NetworkAnonymizationKeyTest
    : public testing::Test,
      public testing::WithParamInterface<EnabledFeatureFlagsTestingParam> {
 public:
  void SetUp() override {
    std::vector<base::Feature> enabled_features = {};
    std::vector<base::Feature> disabled_features = {};

    if (IsDoubleKeyEnabled()) {
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

  bool IsDoubleKeyEnabled() {
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
  if (IsDoubleKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_FALSE(NetworkAnonymizationKey::IsFrameSiteEnabled());
  } else {
    EXPECT_TRUE(NetworkAnonymizationKey::IsFrameSiteEnabled());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsDoubleKeySchemeEnabled) {
  // Double key scheme is enabled only when
  // `kEnableDoubleKeyNetworkAnonymizationKeyEnabled` is enabled but
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is not.
  if (IsDoubleKeyEnabled() && !IsCrossSiteFlagEnabled()) {
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

TEST_P(NetworkAnonymizationKeyTest, IsEmpty) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsEmpty());
  EXPECT_FALSE(populated_key.IsEmpty());
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
  NetworkAnonymizationKey populated_double_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/absl::nullopt,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsTransient());
  EXPECT_FALSE(populated_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(populated_key_with_nonce.IsTransient());

  if (IsDoubleKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_FALSE(data_frame_key.IsTransient());
    EXPECT_FALSE(populated_double_key.IsTransient());
  } else {
    EXPECT_TRUE(data_frame_key.IsTransient());
    EXPECT_TRUE(populated_double_key.IsTransient());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsFullyPopulated) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey empty_frame_site_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/absl::nullopt,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey empty_cross_site_flag_key(
      /*top_frame_site=*/kTestSiteA,
      /*frame_site=*/kTestSiteB,
      /*is_cross_site=*/absl::nullopt,
      /*nonce=*/absl::nullopt);
  EXPECT_TRUE(populated_key.IsFullyPopulated());
  EXPECT_FALSE(empty_key.IsFullyPopulated());
  if (IsDoubleKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_TRUE(empty_frame_site_key.IsFullyPopulated());
  } else {
    EXPECT_FALSE(empty_frame_site_key.IsFullyPopulated());
  }

  // is_cross_site is required when
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
  if (IsCrossSiteFlagEnabled()) {
    EXPECT_FALSE(empty_cross_site_flag_key.IsFullyPopulated());
  } else {
    EXPECT_TRUE(empty_cross_site_flag_key.IsFullyPopulated());
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
  if (IsDoubleKeyEnabled() || IsCrossSiteFlagEnabled()) {
    EXPECT_EQ(key.GetFrameSite(), absl::nullopt);
  } else {
    EXPECT_EQ(key.GetFrameSite(), kTestSiteB);
  }

  // is_cross_site should only be true when
  // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
  if (IsCrossSiteFlagEnabled()) {
    EXPECT_TRUE(key.GetIsCrossSite());
  } else {
    EXPECT_DEATH_IF_SUPPORTED(key.GetIsCrossSite(), "");
  }
}

TEST_P(NetworkAnonymizationKeyTest, ToDebugString) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);
  NetworkAnonymizationKey empty_key;

  if (IsDoubleKeyEnabled() && !IsCrossSiteFlagEnabled()) {
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
    // is_cross_site_ must be populated if
    // `kEnableCrossSiteFlagNetworkAnonymizationKey` is enabled.
    EXPECT_DEATH_IF_SUPPORTED(empty_key.ToDebugString(), "");
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

  if (IsDoubleKeyEnabled() || IsCrossSiteFlagEnabled()) {
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

}  // namespace net