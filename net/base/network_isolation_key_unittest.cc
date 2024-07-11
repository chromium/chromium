// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/network_isolation_key.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

namespace {
const char kDataUrl[] = "data:text/html,<body>Hello World</body>";

TEST(NetworkIsolationKeyTest, EmptyKey) {
  NetworkIsolationKey key;
  EXPECT_FALSE(key.IsFullyPopulated());
  EXPECT_EQ(std::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ("null null", key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, NonEmptySameSiteKey) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  NetworkIsolationKey key(site1, site1);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(site1.Serialize() + " " + site1.Serialize(),
            key.ToCacheKeyString());
  EXPECT_EQ(site1.GetDebugString() + " " + site1.GetDebugString(),
            key.ToDebugString());
  EXPECT_FALSE(key.IsTransient());
}

TEST(NetworkIsolationKeyTest, NonEmptyCrossSiteKey) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site2 = SchemefulSite(GURL("http://b.test/"));
  NetworkIsolationKey key(site1, site2);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(site1.Serialize() + " " + site2.Serialize(),
            key.ToCacheKeyString());
  EXPECT_EQ(site1.GetDebugString() + " " + site2.GetDebugString(),
            key.ToDebugString());
  EXPECT_FALSE(key.IsTransient());
}

TEST(NetworkIsolationKeyTest, KeyWithNonce) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site2 = SchemefulSite(GURL("http://b.test/"));
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  NetworkIsolationKey key(site1, site2, nonce);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ(site1.GetDebugString() + " " + site2.GetDebugString() +
                " (with nonce " + nonce.ToString() + ")",
            key.ToDebugString());

  // Create another NetworkIsolationKey with the same input parameters, and
  // check that it is equal.
  NetworkIsolationKey same_key(site1, site2, nonce);
  EXPECT_EQ(key, same_key);

  // Create another NetworkIsolationKey with a different nonce and check that
  // it's different.
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  NetworkIsolationKey key2(site1, site2, nonce2);
  EXPECT_NE(key, key2);
  EXPECT_NE(key.ToDebugString(), key2.ToDebugString());
}

TEST(NetworkIsolationKeyTest, OpaqueOriginKey) {
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key(site_data, site_data);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ(site_data.GetDebugString() + " " + site_data.GetDebugString(),
            key.ToDebugString());

  // Create another site with an opaque origin, and make sure it's different and
  // has a different debug string.
  SchemefulSite other_site = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey other_key(other_site, other_site);
  EXPECT_NE(key, other_key);
  EXPECT_NE(key.ToDebugString(), other_key.ToDebugString());
  EXPECT_EQ(other_site.GetDebugString() + " " + other_site.GetDebugString(),
            other_key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, OpaqueOriginTopLevelSiteKey) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key(site_data, site1);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ(site_data.GetDebugString() + " " + site1.GetDebugString(),
            key.ToDebugString());

  // Create another site with an opaque origin, and make sure it's different and
  // has a different debug string.
  SchemefulSite other_site = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey other_key(other_site, site1);
  EXPECT_NE(key, other_key);
  EXPECT_NE(key.ToDebugString(), other_key.ToDebugString());
  EXPECT_EQ(other_site.GetDebugString() + " " + site1.GetDebugString(),
            other_key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, OpaqueOriginIframeKey) {
  SchemefulSite site1 = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key(site1, site_data);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::nullopt, key.ToCacheKeyString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ(site1.GetDebugString() + " " + site_data.GetDebugString(),
            key.ToDebugString());

  // Create another site with an opaque origin iframe, and make sure it's
  // different and has a different debug string when the frame site is in use.
  SchemefulSite other_site = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey other_key(site1, other_site);
  EXPECT_NE(key, other_key);
  EXPECT_NE(key.ToDebugString(), other_key.ToDebugString());
  EXPECT_EQ(site1.GetDebugString() + " " + other_site.GetDebugString(),
            other_key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, Operators) {
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
                          SchemefulSite(GURL("https://a.test/")), nonce1),
      NetworkIsolationKey(SchemefulSite(GURL("https://a.test/")),
                          SchemefulSite(GURL("https://a.test/")), nonce2),
  };

  for (size_t first = 0; first < std::size(kKeys); ++first) {
    NetworkIsolationKey key1 = kKeys[first];
    SCOPED_TRACE(key1.ToDebugString());

    EXPECT_TRUE(key1 == key1);
    EXPECT_FALSE(key1 != key1);
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

TEST(NetworkIsolationKeyTest, UniqueOriginOperators) {
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

TEST(NetworkIsolationKeyTest, OpaqueSiteKeyBoth) {
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
  EXPECT_FALSE(key1 < key2 || key2 < key1);
  EXPECT_FALSE(key1 == key3);
  EXPECT_TRUE(key1 < key3 || key3 < key1);
  EXPECT_NE(key1.ToDebugString(), key3.ToDebugString());

  // Test the ToString and ToDebugString
  EXPECT_EQ(key1.ToDebugString(), key2.ToDebugString());
  EXPECT_EQ(std::nullopt, key1.ToCacheKeyString());
  EXPECT_EQ(std::nullopt, key2.ToCacheKeyString());
  EXPECT_EQ(std::nullopt, key3.ToCacheKeyString());
}

// Make sure that the logic to extract the registerable domain from an origin
// does not affect the host when using a non-standard scheme.
TEST(NetworkIsolationKeyTest, NonStandardScheme) {
  // Have to register the scheme, or SchemefulSite() will return an opaque
  // origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  SchemefulSite site = SchemefulSite(GURL("foo://a.foo.com"));
  NetworkIsolationKey key(site, site);
  EXPECT_FALSE(key.GetTopFrameSite()->opaque());
  EXPECT_EQ("foo://a.foo.com foo://a.foo.com", key.ToCacheKeyString());
}

TEST(NetworkIsolationKeyTest, CreateWithNewFrameSite) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.com"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.com"));
  SchemefulSite site_c = SchemefulSite(GURL("http://c.com"));

  NetworkIsolationKey key(site_a, site_b);
  NetworkIsolationKey key_c = key.CreateWithNewFrameSite(site_c);
  EXPECT_EQ(site_c, key_c.GetFrameSiteForTesting());
  EXPECT_NE(key_c, key);
  EXPECT_EQ(site_a, key_c.GetTopFrameSite());

  // Ensure that `CreateWithNewFrameSite()` preserves the nonce if one exists.
  base::UnguessableToken nonce = base::UnguessableToken::Create();
  NetworkIsolationKey key_with_nonce(site_a, site_b, nonce);
  NetworkIsolationKey key_with_nonce_c =
      key_with_nonce.CreateWithNewFrameSite(site_c);
  EXPECT_EQ(key_with_nonce.GetNonce(), key_with_nonce_c.GetNonce());
  EXPECT_TRUE(key_with_nonce_c.IsTransient());
}

TEST(NetworkIsolationKeyTest, CreateTransientForTesting) {
  NetworkIsolationKey transient_key =
      NetworkIsolationKey::CreateTransientForTesting();
  EXPECT_TRUE(transient_key.IsFullyPopulated());
  EXPECT_TRUE(transient_key.IsTransient());
  EXPECT_FALSE(transient_key.IsEmpty());
  EXPECT_EQ(transient_key, transient_key);

  // Make sure that subsequent calls don't return the same NIK.
  for (int i = 0; i < 1000; ++i) {
    EXPECT_NE(transient_key, NetworkIsolationKey::CreateTransientForTesting());
  }
}

}  // namespace

}  // namespace net
