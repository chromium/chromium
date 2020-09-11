// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace net {

TEST(NetworkIsolationKeyTest, EmptyKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key;
  EXPECT_FALSE(key.IsFullyPopulated());
  EXPECT_EQ(std::string(), key.ToString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ("null", key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, NonEmptyKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  url::Origin origin = url::Origin::Create(GURL("http://a.test/"));
  NetworkIsolationKey key(origin, origin);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(origin.Serialize(), key.ToString());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_EQ("http://a.test", key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, OpaqueOriginKey) {
  url::Origin origin_data =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));
  NetworkIsolationKey key(origin_data, origin_data);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::string(), key.ToString());
  EXPECT_TRUE(key.IsTransient());

  // Create another opaque origin, and make sure it has a different debug
  // string.
  const auto kOriginNew = origin_data.DeriveNewOpaqueOrigin();
  EXPECT_NE(key.ToDebugString(),
            NetworkIsolationKey(kOriginNew, kOriginNew).ToDebugString());
}

TEST(NetworkIsolationKeyTest, Operators) {
  // These are in ascending order.
  const NetworkIsolationKey kKeys[] = {
      NetworkIsolationKey(),
      // Unique origins are still sorted by scheme, so data is before file, and
      // file before http.
      NetworkIsolationKey(
          url::Origin::Create(GURL("data:text/html,<body>Hello World</body>")),
          url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"))),
      NetworkIsolationKey(url::Origin::Create(GURL("file:///foo")),
                          url::Origin::Create(GURL("file:///foo"))),
      NetworkIsolationKey(url::Origin::Create(GURL("http://a.test/")),
                          url::Origin::Create(GURL("http://a.test/"))),
      NetworkIsolationKey(url::Origin::Create(GURL("http://b.test/")),
                          url::Origin::Create(GURL("http://b.test/"))),
      NetworkIsolationKey(url::Origin::Create(GURL("https://a.test/")),
                          url::Origin::Create(GURL("https://a.test/"))),
  };

  for (size_t first = 0; first < base::size(kKeys); ++first) {
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

    for (size_t second = first + 1; second < base::size(kKeys); ++second) {
      NetworkIsolationKey key2 = kKeys[second];
      SCOPED_TRACE(key2.ToDebugString());

      EXPECT_TRUE(key1 < key2);
      EXPECT_FALSE(key2 < key1);
      EXPECT_FALSE(key1 == key2);
      EXPECT_FALSE(key2 == key1);
    }
  }
}

TEST(NetworkIsolationKeyTest, UniqueOriginOperators) {
  const auto kOrigin1 =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));
  const auto kOrigin2 =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));
  NetworkIsolationKey key1(kOrigin1, kOrigin1);
  NetworkIsolationKey key2(kOrigin2, kOrigin2);

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

TEST(NetworkIsolationKeyTest, KeyWithOpaqueFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  url::Origin origin_data =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));

  NetworkIsolationKey key1(url::Origin::Create(GURL("http://a.test")),
                           origin_data);
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_FALSE(key1.IsTransient());
  EXPECT_EQ("http://a.test", key1.ToString());
  EXPECT_EQ("http://a.test", key1.ToDebugString());

  NetworkIsolationKey key2(origin_data,
                           url::Origin::Create(GURL("http://a.test")));
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_EQ("", key2.ToString());
  EXPECT_EQ(origin_data.GetDebugString(), key2.ToDebugString());
  EXPECT_NE(origin_data.DeriveNewOpaqueOrigin().GetDebugString(),
            key2.ToDebugString());
}

TEST(NetworkIsolationKeyTest, ValueRoundTripEmpty) {
  const url::Origin kJunkOrigin =
      url::Origin::Create(GURL("data:text/html,junk"));

  for (bool use_frame_origins : {true, false}) {
    SCOPED_TRACE(use_frame_origins);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_origins) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    // Convert empty key to value and back, expecting the same value.
    NetworkIsolationKey no_frame_origin_key;
    base::Value no_frame_origin_value;
    ASSERT_TRUE(no_frame_origin_key.ToValue(&no_frame_origin_value));

    // Fill initial value with junk data, to make sure it's overwritten.
    NetworkIsolationKey out_key(kJunkOrigin, kJunkOrigin);
    EXPECT_TRUE(
        NetworkIsolationKey::FromValue(no_frame_origin_value, &out_key));
    EXPECT_EQ(no_frame_origin_key, out_key);
  }
}

TEST(NetworkIsolationKeyTest, ValueRoundTripNoFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);
  const url::Origin kJunkOrigin =
      url::Origin::Create(GURL("data:text/html,junk"));

  NetworkIsolationKey key1(url::Origin::Create(GURL("https://foo.test/")),
                           kJunkOrigin);
  base::Value value;
  ASSERT_TRUE(key1.ToValue(&value));

  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey key2(kJunkOrigin, kJunkOrigin);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(value, &key2));
  EXPECT_EQ(key1, key2);

  feature_list.Reset();
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Loading should fail when frame origins are enabled.
  EXPECT_FALSE(NetworkIsolationKey::FromValue(value, &key2));
}

TEST(NetworkIsolationKeyTest, ValueRoundTripFrameOrigin) {
  const url::Origin kJunkOrigin =
      url::Origin::Create(GURL("data:text/html,junk"));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key1(url::Origin::Create(GURL("https://foo.test/")),
                           url::Origin::Create(GURL("https://foo.test/")));
  base::Value value;
  ASSERT_TRUE(key1.ToValue(&value));

  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey key2(kJunkOrigin, kJunkOrigin);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(value, &key2));
  EXPECT_EQ(key1, key2);

  feature_list.Reset();
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Loading should fail when frame origins are disabled.
  EXPECT_FALSE(NetworkIsolationKey::FromValue(value, &key2));
}

TEST(NetworkIsolationKeyTest, ToValueTransientOrigin) {
  const url::Origin kTransientOrigin =
      url::Origin::Create(GURL("data:text/html,transient"));

  for (bool use_frame_origins : {true, false}) {
    SCOPED_TRACE(use_frame_origins);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_origins) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    NetworkIsolationKey key1(kTransientOrigin, kTransientOrigin);
    EXPECT_TRUE(key1.IsTransient());
    base::Value value;
    EXPECT_FALSE(key1.ToValue(&value));
  }
}

TEST(NetworkIsolationKeyTest, FromValueBadData) {
  // Can't create these inline, since vector initialization lists require a
  // copy, and base::Value has no copy operator, only move.
  base::Value::ListStorage not_a_url_list;
  not_a_url_list.emplace_back(base::Value("not-a-url"));

  base::Value::ListStorage transient_origin_list;
  transient_origin_list.emplace_back(base::Value("data:text/html,transient"));

  base::Value::ListStorage too_many_origins_list;
  too_many_origins_list.emplace_back(base::Value("https://too/"));
  too_many_origins_list.emplace_back(base::Value("https://many/"));
  too_many_origins_list.emplace_back(base::Value("https://origins/"));

  const base::Value kTestCases[] = {
      base::Value(base::Value::Type::STRING),
      base::Value(base::Value::Type::DICTIONARY),
      base::Value(std::move(not_a_url_list)),
      base::Value(std::move(transient_origin_list)),
      base::Value(std::move(too_many_origins_list)),
  };

  for (bool use_frame_origins : {true, false}) {
    SCOPED_TRACE(use_frame_origins);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_origins) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    for (const auto& test_case : kTestCases) {
      NetworkIsolationKey key;
      // Write the value on failure.
      EXPECT_FALSE(NetworkIsolationKey::FromValue(test_case, &key))
          << test_case;
    }
  }
}

TEST(NetworkIsolationKeyTest, UseRegistrableDomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Both origins are non-opaque.
  url::Origin origin_a = url::Origin::Create(GURL("http://a.foo.test:80"));
  url::Origin origin_b = url::Origin::Create(GURL("https://b.foo.test:2395"));

  // Resultant NIK should have the same scheme as the initial origin and
  // default port. Note that frame_origin will be empty as triple keying is not
  // enabled.
  url::Origin expected_domain_a = url::Origin::Create(GURL("http://foo.test"));
  NetworkIsolationKey key(origin_a, origin_b);
  EXPECT_EQ(origin_a, key.GetTopFrameOrigin().value());
  EXPECT_FALSE(key.GetFrameOrigin().has_value());
  EXPECT_EQ(expected_domain_a.Serialize(), key.ToString());

  // More tests for using registrable domain are in
  // NetworkIsolationKeyWithFrameOriginTest.UseRegistrableDomain.
}

class OpaqueNonTransientNetworkIsolationKeyTest : public testing::Test {
 public:
  OpaqueNonTransientNetworkIsolationKeyTest() = default;
  ~OpaqueNonTransientNetworkIsolationKeyTest() override = default;

  std::string GetOriginNonceToString(const net::NetworkIsolationKey& key) {
    return key.GetTopFrameOrigin().value().nonce_->token().ToString();
  }
};

TEST_F(OpaqueNonTransientNetworkIsolationKeyTest,
       OpaqueNonTransient_DisableAppendFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key = NetworkIsolationKey::CreateOpaqueAndNonTransient();
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_FALSE(key.IsEmpty());
  EXPECT_EQ("opaque non-transient " + GetOriginNonceToString(key),
            key.ToString());
  EXPECT_EQ(key.GetTopFrameOrigin()->GetDebugString() + " non-transient",
            key.ToDebugString());

  // |opaque_and_non_transient_| is kept when a new frame origin is opaque.
  url::Origin opaque_origin;
  NetworkIsolationKey new_frame_origin =
      key.CreateWithNewFrameOrigin(opaque_origin);
  EXPECT_TRUE(new_frame_origin.IsFullyPopulated());
  EXPECT_FALSE(new_frame_origin.IsTransient());
  EXPECT_FALSE(new_frame_origin.IsEmpty());
  EXPECT_EQ("opaque non-transient " + GetOriginNonceToString(new_frame_origin),
            new_frame_origin.ToString());
  EXPECT_EQ(
      new_frame_origin.GetTopFrameOrigin()->GetDebugString() + " non-transient",
      new_frame_origin.ToDebugString());

  // Should not be equal to a similar NetworkIsolationKey derived from it.
  EXPECT_NE(key, NetworkIsolationKey(*key.GetTopFrameOrigin(),
                                     *key.GetTopFrameOrigin()));

  // To and back from a Value should yield the same key.
  base::Value value;
  ASSERT_TRUE(key.ToValue(&value));
  NetworkIsolationKey from_value;
  ASSERT_TRUE(NetworkIsolationKey::FromValue(value, &from_value));
  EXPECT_EQ(key, from_value);
  EXPECT_EQ(key.ToString(), from_value.ToString());
  EXPECT_EQ(key.ToDebugString(), from_value.ToDebugString());
}

TEST_F(OpaqueNonTransientNetworkIsolationKeyTest,
       OpaqueNonTransient_EnableAppendFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key = NetworkIsolationKey::CreateOpaqueAndNonTransient();
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_FALSE(key.IsEmpty());
  EXPECT_EQ("opaque non-transient " + GetOriginNonceToString(key),
            key.ToString());
  EXPECT_EQ(key.GetTopFrameOrigin()->GetDebugString() + " " +
                key.GetFrameOrigin()->GetDebugString() + " non-transient",
            key.ToDebugString());

  // |opaque_and_non_transient_| is kept when a new frame origin is opaque.
  url::Origin opaque_origin;
  NetworkIsolationKey new_frame_origin =
      key.CreateWithNewFrameOrigin(opaque_origin);
  EXPECT_TRUE(new_frame_origin.IsFullyPopulated());
  EXPECT_FALSE(new_frame_origin.IsTransient());
  EXPECT_FALSE(new_frame_origin.IsEmpty());
  EXPECT_EQ("opaque non-transient " + GetOriginNonceToString(new_frame_origin),
            new_frame_origin.ToString());
  EXPECT_EQ(new_frame_origin.GetTopFrameOrigin()->GetDebugString() + " " +
                new_frame_origin.GetFrameOrigin()->GetDebugString() +
                " non-transient",
            new_frame_origin.ToDebugString());

  // Should not be equal to a similar NetworkIsolationKey derived from it.
  EXPECT_NE(key, NetworkIsolationKey(*key.GetTopFrameOrigin(),
                                     *key.GetFrameOrigin()));

  // To and back from a Value should yield the same key.
  base::Value value;
  ASSERT_TRUE(key.ToValue(&value));
  NetworkIsolationKey from_value;
  ASSERT_TRUE(NetworkIsolationKey::FromValue(value, &from_value));
  EXPECT_EQ(key, from_value);
  EXPECT_EQ(key.ToString(), from_value.ToString());
  EXPECT_EQ(key.ToDebugString(), from_value.ToDebugString());
}

class NetworkIsolationKeyWithFrameOriginTest : public testing::Test {
 public:
  NetworkIsolationKeyWithFrameOriginTest() {
    feature_list_.InitAndEnableFeature(
        features::kAppendFrameOriginToNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NetworkIsolationKeyWithFrameOriginTest, WithFrameOrigin) {
  NetworkIsolationKey key(url::Origin::Create(GURL("http://b.test")),
                          url::Origin::Create(GURL("http://a.test/")));
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_EQ("http://b.test http://a.test", key.ToString());
  EXPECT_EQ("http://b.test http://a.test", key.ToDebugString());

  EXPECT_TRUE(key == key);
  EXPECT_FALSE(key != key);
  EXPECT_FALSE(key < key);
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, OpaqueOriginKey) {
  url::Origin origin_data =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));

  NetworkIsolationKey key1(url::Origin::Create(GURL("http://a.test")),
                           origin_data);
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_TRUE(key1.IsTransient());
  EXPECT_EQ("", key1.ToString());
  EXPECT_EQ("http://a.test " + origin_data.GetDebugString(),
            key1.ToDebugString());
  EXPECT_NE(
      "http://a.test " + origin_data.DeriveNewOpaqueOrigin().GetDebugString(),
      key1.ToDebugString());

  NetworkIsolationKey key2(origin_data,
                           url::Origin::Create(GURL("http://a.test")));
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_EQ("", key2.ToString());
  EXPECT_EQ(origin_data.GetDebugString() + " http://a.test",
            key2.ToDebugString());
  EXPECT_NE(
      origin_data.DeriveNewOpaqueOrigin().GetDebugString() + " http://a.test",
      key2.ToDebugString());
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, OpaqueOriginKeyBoth) {
  url::Origin origin_data_1 =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));
  url::Origin origin_data_2 =
      url::Origin::Create(GURL("data:text/html,<body>Hello Universe</body>"));
  url::Origin origin_data_3 =
      url::Origin::Create(GURL("data:text/html,<body>Hello Cosmos</body>"));

  NetworkIsolationKey key1(origin_data_1, origin_data_2);
  NetworkIsolationKey key2(origin_data_1, origin_data_2);
  NetworkIsolationKey key3(origin_data_1, origin_data_3);

  // All the keys should be fully populated and transient.
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key3.IsFullyPopulated());
  EXPECT_TRUE(key1.IsTransient());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_TRUE(key3.IsTransient());

  // Test the equality/comparisons of the various keys
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 == key3);
  EXPECT_FALSE(key1 < key2 || key2 < key1);
  EXPECT_TRUE(key1 < key3 || key3 < key1);

  // Test the ToString and ToDebugString
  EXPECT_EQ(key1.ToDebugString(), key2.ToDebugString());
  EXPECT_NE(key1.ToDebugString(), key3.ToDebugString());
  EXPECT_EQ("", key1.ToString());
  EXPECT_EQ("", key2.ToString());
  EXPECT_EQ("", key3.ToString());
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, UseRegistrableDomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Both origins are non-opaque.
  url::Origin origin_a = url::Origin::Create(GURL("http://a.foo.test:80"));
  url::Origin origin_b = url::Origin::Create(GURL("https://b.foo.test:2395"));

  // Resultant NIK should have the same schemes as the initial origins and
  // default port.
  url::Origin expected_domain_a = url::Origin::Create(GURL("http://foo.test"));
  url::Origin expected_domain_b = url::Origin::Create(GURL("https://foo.test"));
  NetworkIsolationKey key(origin_a, origin_b);
  EXPECT_EQ(origin_a, key.GetTopFrameOrigin().value());
  EXPECT_EQ(origin_b, key.GetFrameOrigin().value());
  EXPECT_EQ(expected_domain_a.Serialize() + " " + expected_domain_b.Serialize(),
            key.ToString());

  // Top frame origin is opaque but not the frame origin.
  url::Origin origin_data =
      url::Origin::Create(GURL("data:text/html,<body>Hello World</body>"));
  key = NetworkIsolationKey(origin_data, origin_b);
  EXPECT_TRUE(key.top_frame_origin_->opaque());
  EXPECT_TRUE(key.ToString().empty());
  EXPECT_EQ(origin_data, key.top_frame_origin_.value());
  EXPECT_EQ(expected_domain_b, key.frame_origin_.value());

  // Top frame origin is non-opaque but frame origin is opaque.
  key = NetworkIsolationKey(origin_a, origin_data);
  EXPECT_EQ(expected_domain_a, key.top_frame_origin_.value());
  EXPECT_TRUE(key.ToString().empty());
  EXPECT_EQ(origin_data, key.GetFrameOrigin().value());
  EXPECT_TRUE(key.frame_origin_->opaque());

  // Empty NIK stays empty.
  NetworkIsolationKey empty_key;
  EXPECT_TRUE(key.ToString().empty());

  // IPv4 and IPv6 origins should not be modified, except for removing their
  // ports.
  url::Origin origin_ipv4 = url::Origin::Create(GURL("http://127.0.0.1:1234"));
  url::Origin origin_ipv6 = url::Origin::Create(GURL("https://[::1]"));
  key = NetworkIsolationKey(origin_ipv4, origin_ipv6);
  EXPECT_EQ(url::Origin::Create(GURL("http://127.0.0.1")),
            key.top_frame_origin_.value());
  EXPECT_EQ(origin_ipv6, key.frame_origin_.value());

  // Nor should TLDs, recognized or not.
  url::Origin origin_tld = url::Origin::Create(GURL("http://com"));
  url::Origin origin_tld_unknown =
      url::Origin::Create(GURL("https://bar:1234"));
  key = NetworkIsolationKey(origin_tld, origin_tld_unknown);
  EXPECT_EQ(origin_tld, key.top_frame_origin_.value());
  EXPECT_EQ(url::Origin::Create(GURL("https://bar")),
            key.frame_origin_.value());

  // Check for two-part TLDs.
  url::Origin origin_two_part_tld = url::Origin::Create(GURL("http://co.uk"));
  url::Origin origin_two_part_tld_with_prefix =
      url::Origin::Create(GURL("https://a.b.co.uk"));
  key =
      NetworkIsolationKey(origin_two_part_tld, origin_two_part_tld_with_prefix);
  EXPECT_EQ(origin_two_part_tld, key.top_frame_origin_.value());
  EXPECT_EQ(url::Origin::Create(GURL("https://b.co.uk")),
            key.frame_origin_.value());

  // Two keys with different origins but same etld+1.
  // Also test the getter APIs.
  url::Origin origin_a_foo = url::Origin::Create(GURL("http://a.foo.com"));
  url::Origin foo = url::Origin::Create(GURL("http://foo.com"));
  url::Origin origin_b_foo = url::Origin::Create(GURL("http://b.foo.com"));
  NetworkIsolationKey key1 = NetworkIsolationKey(origin_a_foo, origin_a_foo);
  NetworkIsolationKey key2 = NetworkIsolationKey(origin_b_foo, origin_b_foo);
  EXPECT_EQ(key1, key2);
  EXPECT_EQ(foo.Serialize() + " " + foo.Serialize(), key1.ToString());
  EXPECT_EQ(foo.Serialize() + " " + foo.Serialize(), key2.ToString());
  EXPECT_EQ(origin_a_foo, key1.GetTopFrameOrigin());
  EXPECT_EQ(origin_a_foo, key1.GetFrameOrigin());
  EXPECT_EQ(origin_b_foo, key2.GetTopFrameOrigin());
  EXPECT_EQ(origin_b_foo, key2.GetFrameOrigin());

  // Copying one key to another should also copy the original origins.
  url::Origin origin_bar = url::Origin::Create(GURL("http://a.bar.com"));
  NetworkIsolationKey key_bar = NetworkIsolationKey(origin_bar, origin_bar);
  NetworkIsolationKey key_copied = key_bar;
  EXPECT_EQ(key_copied.GetTopFrameOrigin(), key_bar.GetTopFrameOrigin());
  EXPECT_EQ(key_copied.GetFrameOrigin(), key_bar.GetFrameOrigin());
  EXPECT_EQ(key_copied, key_bar);
}

// Make sure that the logic to extract the registerable domain from an origin
// does not affect the host when using a non-standard scheme.
TEST(NetworkIsolationKeyTest, NonStandardScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Have to register the scheme, or url::Origin::Create() will return an opaque
  // origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  url::Origin origin = url::Origin::Create(GURL("foo://a.foo.com"));
  ASSERT_FALSE(origin.opaque());
  ASSERT_EQ(origin.scheme(), "foo");
  ASSERT_EQ(origin.host(), "a.foo.com");

  net::NetworkIsolationKey key(origin, origin);
  EXPECT_EQ(origin, key.GetTopFrameOrigin());
  EXPECT_FALSE(key.GetTopFrameOrigin()->opaque());
  EXPECT_EQ(key.GetTopFrameOrigin()->scheme(), "foo");
  EXPECT_EQ(key.GetTopFrameOrigin()->host(), "a.foo.com");
  EXPECT_EQ(origin.Serialize(), key.ToString());
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, CreateWithNewFrameOrigin) {
  url::Origin origin_a = url::Origin::Create(GURL("http://a.com"));
  url::Origin origin_b = url::Origin::Create(GURL("http://b.com"));
  url::Origin origin_c = url::Origin::Create(GURL("http://c.com"));

  net::NetworkIsolationKey key(origin_a, origin_b);
  NetworkIsolationKey key_c = key.CreateWithNewFrameOrigin(origin_c);
  EXPECT_EQ(origin_c, key_c.GetFrameOrigin());
  EXPECT_EQ(origin_a, key_c.GetTopFrameOrigin());
}

TEST(NetworkIsolationKeyTest, CreateTransient) {
  for (bool use_frame_origins : {true, false}) {
    SCOPED_TRACE(use_frame_origins);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_origins) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

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
}

}  // namespace net
