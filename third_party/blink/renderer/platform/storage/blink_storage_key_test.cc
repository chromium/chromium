// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "url/gurl.h"

namespace blink {

TEST(BlinkStorageKeyTest, OpaqueOriginsDistinct) {
  // Test that two opaque origins give distinct BlinkStorageKeys.
  BlinkStorageKey unique_opaque1;
  EXPECT_TRUE(unique_opaque1.GetSecurityOrigin());
  EXPECT_TRUE(unique_opaque1.GetSecurityOrigin()->IsOpaque());
  BlinkStorageKey unique_opaque2;
  EXPECT_FALSE(unique_opaque2.GetSecurityOrigin()->IsSameOriginWith(
      unique_opaque1.GetSecurityOrigin().get()));
  EXPECT_NE(unique_opaque1, unique_opaque2);
}

TEST(BlinkStorageKeyTest, EqualityWithNonce) {
  // Test that BlinkStorageKeys with different nonces are different.
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://example.com");
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  BlinkStorageKey key1 = BlinkStorageKey::CreateWithNonce(origin, token1);
  BlinkStorageKey key2 = BlinkStorageKey::CreateWithNonce(origin, token1);
  BlinkStorageKey key3 = BlinkStorageKey::CreateWithNonce(origin, token2);

  EXPECT_TRUE(key1.GetSecurityOrigin()->IsSameOriginWith(
      key2.GetSecurityOrigin().get()));
  EXPECT_TRUE(key1.GetSecurityOrigin()->IsSameOriginWith(
      key3.GetSecurityOrigin().get()));
  EXPECT_TRUE(key2.GetSecurityOrigin()->IsSameOriginWith(
      key3.GetSecurityOrigin().get()));

  EXPECT_EQ(key1, key2);
  EXPECT_NE(key1, key3);
}

TEST(BlinkStorageKeyTest, OpaqueOriginRetained) {
  // Test that a StorageKey made from an opaque origin retains the origin.
  scoped_refptr<const SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> opaque_copied =
      opaque_origin->IsolatedCopy();
  BlinkStorageKey from_opaque(std::move(opaque_origin));
  EXPECT_TRUE(
      from_opaque.GetSecurityOrigin()->IsSameOriginWith(opaque_copied.get()));
}

TEST(BlinkStorageKeyTest, CreateFromNonOpaqueOrigin) {
  struct {
    const char* origin;
  } kTestCases[] = {
      {"http://example.site"},
      {"https://example.site"},
      {"file:///path/to/file"},
  };

  for (const auto& test : kTestCases) {
    scoped_refptr<const SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.origin);
    ASSERT_FALSE(origin->IsOpaque());
    scoped_refptr<const SecurityOrigin> copied = origin->IsolatedCopy();

    // Test that the origin is retained.
    BlinkStorageKey storage_key(std::move(origin));
    EXPECT_TRUE(
        storage_key.GetSecurityOrigin()->IsSameOriginWith(copied.get()));

    // Test that two StorageKeys from the same origin are the same.
    BlinkStorageKey storage_key_from_copy(std::move(copied));
    EXPECT_EQ(storage_key, storage_key_from_copy);
  }
}

// Tests that the conversion BlinkStorageKey -> StorageKey -> BlinkStorageKey is
// the identity.
TEST(BlinkStorageKeyTest, BlinkStorageKeyRoundTripConversion) {
  scoped_refptr<const SecurityOrigin> origin1 =
      SecurityOrigin::CreateUniqueOpaque();
  scoped_refptr<const SecurityOrigin> origin2 =
      SecurityOrigin::CreateFromString("http://example.site");
  scoped_refptr<const SecurityOrigin> origin3 =
      SecurityOrigin::CreateFromString("https://example.site");
  scoped_refptr<const SecurityOrigin> origin4 =
      SecurityOrigin::CreateFromString("file:///path/to/file");
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    Vector<BlinkStorageKey> keys = {
        BlinkStorageKey(),
        BlinkStorageKey(origin1),
        BlinkStorageKey(origin2),
        BlinkStorageKey(origin3),
        BlinkStorageKey(origin4),
        BlinkStorageKey::CreateWithNonce(origin1, nonce),
        BlinkStorageKey::CreateWithNonce(origin2, nonce),
        BlinkStorageKey(origin1, BlinkSchemefulSite(origin2), nullptr,
                        mojom::blink::AncestorChainBit::kCrossSite),
    };

    for (BlinkStorageKey& key : keys) {
      EXPECT_EQ(key, BlinkStorageKey(StorageKey(key)));
      EXPECT_EQ(key.CopyWithForceEnabledThirdPartyStoragePartitioning(),
                BlinkStorageKey(StorageKey(key))
                    .CopyWithForceEnabledThirdPartyStoragePartitioning());
    }
  }
}

// Tests that the conversion StorageKey -> BlinkStorageKey -> StorageKey is the
// identity.
TEST(BlinkStorageKey, StorageKeyRoundTripConversion) {
  url::Origin url_origin1;
  url::Origin url_origin2 = url::Origin::Create(GURL("http://example.site"));
  url::Origin url_origin3 = url::Origin::Create(GURL("https://example.site"));
  url::Origin url_origin4 = url::Origin::Create(GURL("file:///path/to/file"));
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    Vector<StorageKey> storage_keys = {
        StorageKey(url_origin1),
        StorageKey(url_origin2),
        StorageKey(url_origin3),
        StorageKey(url_origin4),
        StorageKey::CreateWithNonce(url_origin1, nonce),
        StorageKey::CreateWithNonce(url_origin2, nonce),
        StorageKey::CreateWithOptionalNonce(
            url_origin1, net::SchemefulSite(url_origin2), nullptr,
            blink::mojom::AncestorChainBit::kCrossSite)};

    for (const auto& key : storage_keys) {
      EXPECT_EQ(key, StorageKey(BlinkStorageKey(key)));
      EXPECT_EQ(key.CopyWithForceEnabledThirdPartyStoragePartitioning(),
                StorageKey(BlinkStorageKey(key))
                    .CopyWithForceEnabledThirdPartyStoragePartitioning());
    }
  }
}

// Test that string -> StorageKey test function performs as expected.
TEST(BlinkStorageKey, CreateFromStringForTesting) {
  WTF::String example = "https://example.com/";
  WTF::String wrong = "I'm not a valid URL.";

  BlinkStorageKey key1 = BlinkStorageKey::CreateFromStringForTesting(example);
  BlinkStorageKey key2 = BlinkStorageKey::CreateFromStringForTesting(wrong);
  BlinkStorageKey key3 =
      BlinkStorageKey::CreateFromStringForTesting(WTF::String());

  EXPECT_FALSE(key1.GetSecurityOrigin()->IsOpaque());
  EXPECT_EQ(key1, BlinkStorageKey(SecurityOrigin::CreateFromString(example)));
  EXPECT_TRUE(key2.GetSecurityOrigin()->IsOpaque());
  EXPECT_TRUE(key3.GetSecurityOrigin()->IsOpaque());
}

// Test that BlinkStorageKey's top_level_site getter returns origin's site when
// storage partitioning is disabled.
TEST(BlinkStorageKey, TopLevelSiteGetter) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key_origin1 = StorageKey(origin1);
  StorageKey key_origin1_site1 = StorageKey::CreateForTesting(origin1, origin1);
  StorageKey key_origin1_site2 = StorageKey::CreateForTesting(origin1, origin2);

  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site2.top_level_site());
}

// Test that BlinkStorageKey's top_level_site getter returns the top level site
// when storage partitioning is enabled.
TEST(BlinkStorageKeyTest, TopLevelSiteGetterWithPartitioningEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  scoped_refptr<const SecurityOrigin> origin1 =
      SecurityOrigin::CreateFromString("https://example.com");
  scoped_refptr<const SecurityOrigin> origin2 =
      SecurityOrigin::CreateFromString("https://test.example");

  BlinkStorageKey key_origin1 = BlinkStorageKey(origin1);
  BlinkStorageKey key_origin1_site1 =
      BlinkStorageKey::CreateForTesting(origin1, BlinkSchemefulSite(origin1));
  BlinkStorageKey key_origin1_site2 =
      BlinkStorageKey::CreateForTesting(origin1, BlinkSchemefulSite(origin2));

  EXPECT_EQ(BlinkSchemefulSite(origin1), key_origin1.GetTopLevelSite());
  EXPECT_EQ(BlinkSchemefulSite(origin1), key_origin1_site1.GetTopLevelSite());
  EXPECT_EQ(BlinkSchemefulSite(origin2), key_origin1_site2.GetTopLevelSite());
}

TEST(BlinkStorageKeyTest, CopyWithForceEnabledThirdPartyStoragePartitioning) {
  scoped_refptr<const SecurityOrigin> origin1 =
      SecurityOrigin::CreateFromString("https://foo.com");
  scoped_refptr<const SecurityOrigin> origin2 =
      SecurityOrigin::CreateFromString("https://bar.com");

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    BlinkStorageKey storage_key(origin1, BlinkSchemefulSite(origin2), nullptr,
                                mojom::blink::AncestorChainBit::kCrossSite);
    EXPECT_EQ(storage_key.GetTopLevelSite(),
              BlinkSchemefulSite(toggle ? origin2 : origin1));
    EXPECT_EQ(storage_key.GetAncestorChainBit(),
              toggle ? mojom::blink::AncestorChainBit::kCrossSite
                     : mojom::blink::AncestorChainBit::kSameSite);

    BlinkStorageKey storage_key_with_3psp =
        storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning();
    EXPECT_EQ(storage_key_with_3psp.GetTopLevelSite(),
              BlinkSchemefulSite(origin2));
    EXPECT_EQ(storage_key_with_3psp.GetAncestorChainBit(),
              mojom::blink::AncestorChainBit::kCrossSite);
  }
}

}  // namespace blink
