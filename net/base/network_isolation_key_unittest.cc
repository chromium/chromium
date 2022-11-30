// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"

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

namespace {
const char kDataUrl[] = "data:text/html,<body>Hello World</body>";

class NetworkIsolationKeyTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
    }
  }
  static bool ForceIsolationInfoFrameOriginToTopLevelFrameEnabled() {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkIsolationKeyTest,
    /*force_isolation_info_frame_origin_to_top_level_frame*/ testing::Bool());

TEST_P(NetworkIsolationKeyTest, IsFrameSiteEnabled) {
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_FALSE(NetworkIsolationKey::IsFrameSiteEnabled());
  } else {
    EXPECT_TRUE(NetworkIsolationKey::IsFrameSiteEnabled());
  }
}

TEST_P(NetworkIsolationKeyTest, EmptyKey) {
  NetworkIsolationKey key;
  EXPECT_FALSE(key.IsFullyPopulated());
  EXPECT_EQ(absl::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ("null null", key.ToDebugString());
}

TEST_P(NetworkIsolationKeyTest, NonEmptyKey) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site2 = SchemefulSite(GURL("http://b.test/"));
  NetworkIsolationKey key(site1, site2);
  EXPECT_TRUE(key.IsFullyPopulated());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_EQ(site1.Serialize() + " " + site1.Serialize(),
              key.ToCacheKeyString());
    EXPECT_EQ(site1.GetDebugString() + " null", key.ToDebugString());
  } else {
    EXPECT_EQ(site1.Serialize() + " " + site2.Serialize(),
              key.ToCacheKeyString());
    EXPECT_EQ(site1.GetDebugString() + " " + site2.GetDebugString(),
              key.ToDebugString());
  }
  EXPECT_FALSE(key.IsTransient());
}

TEST_P(NetworkIsolationKeyTest, KeyWithNonce) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site2 = SchemefulSite(GURL("http://b.test/"));
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  NetworkIsolationKey key(site1, site2, &nonce);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(absl::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_EQ(site1.GetDebugString() + " null" + " (with nonce " +
                  nonce.ToString() + ")",
              key.ToDebugString());
  } else {
    EXPECT_EQ(site1.GetDebugString() + " " + site2.GetDebugString() +
                  " (with nonce " + nonce.ToString() + ")",
              key.ToDebugString());
  }

  // Create another NetworkIsolationKey with the same input parameters, and
  // check that it is equal.
  NetworkIsolationKey same_key(site1, site2, &nonce);
  EXPECT_EQ(key, same_key);

  // Create another NetworkIsolationKey with a different nonce and check that
  // it's different.
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  NetworkIsolationKey key2(site1, site2, &nonce2);
  EXPECT_NE(key, key2);
  EXPECT_NE(key.ToDebugString(), key2.ToDebugString());
}

TEST_P(NetworkIsolationKeyTest, OpaqueOriginKey) {
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key(site_data, site_data);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(absl::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());

  // Create another site with an opaque origin, and make sure it's different and
  // has a different debug string.
  SchemefulSite other_site = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey other_key(other_site, other_site);
  EXPECT_NE(key, other_key);
  EXPECT_NE(key.ToDebugString(), other_key.ToDebugString());
}

TEST_P(NetworkIsolationKeyTest, Operators) {
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  if (nonce2 < nonce1)
    std::swap(nonce1, nonce2);
  // These are in ascending order.
  const NetworkIsolationKey kKeys[] = {
      NetworkIsolationKey(),
      // Site with unique origins are still sorted by scheme, so data is before
      // file, and file before http.
      NetworkIsolationKey(SchemefulSite(GURL(kDataUrl)),
                          SchemefulSite(GURL(kDataUrl))),
      NetworkIsolationKey(SchemefulSite(GURL("file:///foo")),
                          SchemefulSite(GURL("file:///foo"))),
      NetworkIsolationKey(SchemefulSite(GURL("http://a.test/")),
                          SchemefulSite(GURL("http://a.test/"))),
      NetworkIsolationKey(SchemefulSite(GURL("http://b.test/")),
                          SchemefulSite(GURL("http://b.test/"))),
      NetworkIsolationKey(SchemefulSite(GURL("https://a.test/")),
                          SchemefulSite(GURL("https://a.test/"))),
      NetworkIsolationKey(SchemefulSite(GURL("https://a.test/")),
                          SchemefulSite(GURL("https://a.test/")), &nonce1),
      NetworkIsolationKey(SchemefulSite(GURL("https://a.test/")),
                          SchemefulSite(GURL("https://a.test/")), &nonce2),
  };

  for (size_t first = 0; first < std::size(kKeys); ++first) {
    NetworkIsolationKey key1 = kKeys[first];
    SCOPED_TRACE(key1.ToDebugString());

    EXPECT_TRUE(key1 == key1);
    EXPECT_FALSE(key1 < key1);

    // Make sure that copying a key doesn't change the results of any operation.
    // This check is a bit more interesting with unique origins.
    NetworkIsolationKey key1_copy = key1;
    EXPECT_TRUE(key1 == key1_copy);
    EXPECT_FALSE(key1 < key1_copy);
    EXPECT_FALSE(key1_copy < key1);

    for (size_t second = first + 1; second < std::size(kKeys); ++second) {
      NetworkIsolationKey key2 = kKeys[second];
      SCOPED_TRACE(key2.ToDebugString());

      EXPECT_TRUE(key1 < key2);
      EXPECT_FALSE(key2 < key1);
      EXPECT_FALSE(key1 == key2);
      EXPECT_FALSE(key2 == key1);
    }
  }
}

TEST_P(NetworkIsolationKeyTest, UniqueOriginOperators) {
  const auto kSite1 = SchemefulSite(GURL(kDataUrl));
  const auto kSite2 = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key1(kSite1, kSite1);
  NetworkIsolationKey key2(kSite2, kSite2);

  EXPECT_TRUE(key1 == key1);
  EXPECT_TRUE(key2 == key2);

  // Creating copies shouldn't affect comparison result.
  EXPECT_TRUE(NetworkIsolationKey(key1) == NetworkIsolationKey(key1));
  EXPECT_TRUE(NetworkIsolationKey(key2) == NetworkIsolationKey(key2));

  EXPECT_FALSE(key1 == key2);
  EXPECT_FALSE(key2 == key1);

  // Order of Nonces isn't predictable, but they should have an ordering.
  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_TRUE(!(key1 < key2) || !(key2 < key1));
}

TEST_P(NetworkIsolationKeyTest, KeyWithOneOpaqueOrigin) {
  SchemefulSite site = SchemefulSite(GURL("http://a.test"));
  SchemefulSite opaque_site = SchemefulSite(GURL(kDataUrl));

  NetworkIsolationKey key1(site, opaque_site);
  EXPECT_TRUE(key1.IsFullyPopulated());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_FALSE(key1.IsTransient());
    EXPECT_EQ(site.GetDebugString() + " " + site.GetDebugString(),
              key1.ToCacheKeyString());

    EXPECT_EQ(site.GetDebugString() + " null", key1.ToDebugString());
  } else {
    EXPECT_TRUE(key1.IsTransient());
    EXPECT_EQ(absl::nullopt, key1.ToCacheKeyString());
    EXPECT_EQ(site.GetDebugString() + " " + opaque_site.GetDebugString(),
              key1.ToDebugString());
  }

  NetworkIsolationKey key2(opaque_site, site);
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_EQ(absl::nullopt, key2.ToCacheKeyString());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_EQ(opaque_site.GetDebugString() + " null", key2.ToDebugString());
  } else {
    EXPECT_EQ(opaque_site.GetDebugString() + " " + site.GetDebugString(),
              key2.ToDebugString());
  }
}

TEST_P(NetworkIsolationKeyTest, ValueRoundTripEmpty) {
  const SchemefulSite kJunkSite = SchemefulSite(GURL("data:text/html,junk"));

  // Convert empty key to value and back, expecting the same value.
  NetworkIsolationKey no_frame_site_key;
  base::Value no_frame_site_value;
  ASSERT_TRUE(no_frame_site_key.ToValue(&no_frame_site_value));

  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey out_key(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(no_frame_site_value, &out_key));
  EXPECT_EQ(no_frame_site_key, out_key);
}

TEST_P(NetworkIsolationKeyTest, ValueRoundTripNonEmpty) {
  const SchemefulSite kJunkSite = SchemefulSite(GURL("data:text/html,junk"));

  NetworkIsolationKey key1(SchemefulSite(GURL("https://foo.test/")),
                           SchemefulSite(GURL("https://foo.test/")));
  base::Value value;
  ASSERT_TRUE(key1.ToValue(&value));

  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey key2(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(value, &key2));
  EXPECT_EQ(key1, key2);
}

TEST_P(NetworkIsolationKeyTest, ToValueTransientSite) {
  const SchemefulSite kSiteWithTransientOrigin =
      SchemefulSite(GURL("data:text/html,transient"));
  NetworkIsolationKey key(kSiteWithTransientOrigin, kSiteWithTransientOrigin);
  EXPECT_TRUE(key.IsTransient());
  base::Value value;
  EXPECT_FALSE(key.ToValue(&value));
}

TEST_P(NetworkIsolationKeyTest, FromValueBadData) {
  base::Value::List not_a_url_list;
  not_a_url_list.Append("not-a-url");

  base::Value::List transient_origin_list;
  transient_origin_list.Append("data:text/html,transient");

  base::Value::List too_many_origins_list;
  too_many_origins_list.Append("https://too/");
  too_many_origins_list.Append("https://many/");
  too_many_origins_list.Append("https://origins/");

  const base::Value kTestCases[] = {
      base::Value(std::string()),
      base::Value(base::Value::Dict()),
      base::Value(std::move(not_a_url_list)),
      base::Value(std::move(transient_origin_list)),
      base::Value(std::move(too_many_origins_list)),
  };

  for (const auto& test_case : kTestCases) {
    NetworkIsolationKey key;
    // Write the value on failure.
    EXPECT_FALSE(NetworkIsolationKey::FromValue(test_case, &key)) << test_case;
  }

  base::Value::List triple_key_list;
  triple_key_list.Append("http://www.triple.com");
  triple_key_list.Append("http://www.key.com");
  NetworkIsolationKey key;
  base::Value triple_key_case(std::move(triple_key_list));

  // When double key is enabled top_level_site must equal frame_site.
  bool expect_fail_on_different_sites =
      ForceIsolationInfoFrameOriginToTopLevelFrameEnabled();

  if (expect_fail_on_different_sites) {
    EXPECT_FALSE(NetworkIsolationKey::FromValue(triple_key_case, &key))
        << triple_key_case;
  }
}

TEST_P(NetworkIsolationKeyTest, WithFrameSite) {
  NetworkIsolationKey key(SchemefulSite(GURL("http://b.test")),
                          SchemefulSite(GURL("http://a.test/")));
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_EQ("http://b.test http://b.test", key.ToCacheKeyString());
    EXPECT_EQ("http://b.test null", key.ToDebugString());
  } else {
    EXPECT_EQ("http://b.test http://a.test", key.ToCacheKeyString());
    EXPECT_EQ("http://b.test http://a.test", key.ToDebugString());
  }
  EXPECT_TRUE(key == key);
  EXPECT_FALSE(key != key);
  EXPECT_FALSE(key < key);
}

TEST_P(NetworkIsolationKeyTest, OpaqueSiteKey) {
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_data2 = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_a = SchemefulSite(GURL("http://a.test"));

  NetworkIsolationKey key1(site_a, site_data);
  EXPECT_TRUE(key1.IsFullyPopulated());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_FALSE(key1.IsTransient());

    EXPECT_EQ(NetworkIsolationKey(site_a, site_data2), key1);
    EXPECT_EQ("http://a.test http://a.test", key1.ToCacheKeyString());
    EXPECT_EQ("http://a.test null", key1.ToDebugString());
  } else {
    EXPECT_TRUE(key1.IsTransient());

    EXPECT_EQ(absl::nullopt, key1.ToCacheKeyString());
    EXPECT_EQ("http://a.test " + site_data.GetDebugString(),
              key1.ToDebugString());
    EXPECT_NE(NetworkIsolationKey(site_a, site_data2), key1);
  }

  NetworkIsolationKey key2(site_data, site_a);
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_EQ(absl::nullopt, key2.ToCacheKeyString());
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_EQ(site_data.GetDebugString() + " null", key2.ToDebugString());
  } else {
    EXPECT_EQ(site_data.GetDebugString() + " http://a.test",
              key2.ToDebugString());
  }

  EXPECT_NE(NetworkIsolationKey(site_data2, site_a), key2);
}

TEST_P(NetworkIsolationKeyTest, OpaqueSiteKeyBoth) {
  SchemefulSite site_data_1 = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_data_2 = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_data_3 = SchemefulSite(GURL(kDataUrl));

  NetworkIsolationKey key1(site_data_1, site_data_2);
  NetworkIsolationKey key2(site_data_1, site_data_2);
  NetworkIsolationKey key3(site_data_1, site_data_3);

  // All the keys should be fully populated and transient.
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key3.IsFullyPopulated());
  EXPECT_TRUE(key1.IsTransient());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_TRUE(key3.IsTransient());

  // Test the equality/comparisons of the various keys
  EXPECT_TRUE(key1 == key2);
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_TRUE(key1 == key3);
    EXPECT_FALSE(key1 < key3 || key3 < key1);
    EXPECT_EQ(key1.ToDebugString(), key3.ToDebugString());
  } else {
    EXPECT_FALSE(key1 == key3);
    EXPECT_TRUE(key1 < key3 || key3 < key1);
    EXPECT_NE(key1.ToDebugString(), key3.ToDebugString());
  }
  EXPECT_FALSE(key1 < key2 || key2 < key1);

  // Test the ToString and ToDebugString
  EXPECT_EQ(key1.ToDebugString(), key2.ToDebugString());
  EXPECT_EQ(absl::nullopt, key1.ToCacheKeyString());
  EXPECT_EQ(absl::nullopt, key2.ToCacheKeyString());
  EXPECT_EQ(absl::nullopt, key3.ToCacheKeyString());
}

// Make sure that the logic to extract the registerable domain from an origin
// does not affect the host when using a non-standard scheme.
TEST_P(NetworkIsolationKeyTest, NonStandardScheme) {
  // Have to register the scheme, or SchemefulSite() will return an opaque
  // origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  SchemefulSite site = SchemefulSite(GURL("foo://a.foo.com"));
  net::NetworkIsolationKey key(site, site);
  EXPECT_FALSE(key.GetTopFrameSite()->opaque());
  EXPECT_EQ("foo://a.foo.com foo://a.foo.com", key.ToCacheKeyString());
}

TEST_P(NetworkIsolationKeyTest, CreateWithNewFrameSite) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.com"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.com"));
  SchemefulSite site_c = SchemefulSite(GURL("http://c.com"));

  net::NetworkIsolationKey key(site_a, site_b);
  NetworkIsolationKey key_c = key.CreateWithNewFrameSite(site_c);
  if (ForceIsolationInfoFrameOriginToTopLevelFrameEnabled()) {
    EXPECT_DEATH_IF_SUPPORTED(key_c.GetFrameSite(), "");
  } else {
    EXPECT_EQ(site_c, key_c.GetFrameSite());
  }
  EXPECT_EQ(site_a, key_c.GetTopFrameSite());
}

TEST_P(NetworkIsolationKeyTest, CreateTransient) {
  NetworkIsolationKey transient_key = NetworkIsolationKey::CreateTransient();
  EXPECT_TRUE(transient_key.IsFullyPopulated());
  EXPECT_TRUE(transient_key.IsTransient());
  EXPECT_FALSE(transient_key.IsEmpty());
  EXPECT_EQ(transient_key, transient_key);

  // Transient values can't be saved to disk.
  base::Value value;
  EXPECT_FALSE(transient_key.ToValue(&value));

  // Make sure that subsequent calls don't return the same NIK.
  for (int i = 0; i < 1000; ++i) {
    EXPECT_NE(transient_key, NetworkIsolationKey::CreateTransient());
  }
}

TEST(NetworkIsolationKeyFeatureShiftTest, ValueRoundTripDoubleToTriple) {
  base::test::ScopedFeatureList scoped_feature_list_;
  const SchemefulSite kJunkSite = SchemefulSite(GURL("data:text/html,junk"));

  // Turn double keying off.
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
  // Create a triple key.
  NetworkIsolationKey created_triple_key(
      SchemefulSite(GURL("https://foo.test/")),
      SchemefulSite(GURL("https://bar.test/")));

  // Assert round trip of triple key succeeds and key is a correctly formed
  // triple key.
  base::Value created_triple_key_value;
  ASSERT_TRUE(created_triple_key.ToValue(&created_triple_key_value));
  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey created_triple_key2(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(created_triple_key_value,
                                             &created_triple_key2));
  EXPECT_EQ(created_triple_key, created_triple_key2);

  // Serialize a triple key value with frame site enabled.
  base::Value created_triple_key_value2;
  ASSERT_TRUE(created_triple_key.ToValue(&created_triple_key_value2));

  // Turn double keying on.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);

  // Create a key and confirm the frame site is correctly set to nullopt rather
  // than https://bar.test/.
  NetworkIsolationKey created_double_key(
      SchemefulSite(GURL("https://foo.test/")),
      SchemefulSite(GURL("https://bar.test/")));
  EXPECT_DEATH_IF_SUPPORTED(created_double_key.GetFrameSite(), "");

  // Test round trip of key created when frame site was disabled.
  base::Value created_double_key_value;
  ASSERT_TRUE(created_double_key.ToValue(&created_double_key_value));
  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey created_double_key2(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(created_double_key_value,
                                             &created_double_key2));
  EXPECT_EQ(created_double_key, created_double_key2);

  // Test round trip of key created with frame site enabled is now formed
  // correctly as a double key. This key was serialized to value when frame site
  // was enabled and should be able to be created from value without error.
  NetworkIsolationKey created_triple_key3(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(created_triple_key_value2,
                                             &created_triple_key3));
  // Triple key should be in a double key form with the frame site an empty
  // optional.
  EXPECT_EQ(created_double_key, created_triple_key3);
}

}  // namespace

}  // namespace net
