// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
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
        BlinkStorageKey(origin1, BlinkSchemefulSite(), nullptr,
                        mojom::blink::AncestorChainBit::kSameSite),
        BlinkStorageKey(origin2, BlinkSchemefulSite(), nullptr,
                        mojom::blink::AncestorChainBit::kSameSite),
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
        StorageKey::CreateWithNonceForTesting(url_origin1, nonce),
        StorageKey::CreateWithNonceForTesting(url_origin2, nonce),
        StorageKey::CreateWithOptionalNonce(
            url_origin1, net::SchemefulSite(url_origin2), nullptr,
            blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::CreateWithOptionalNonce(
            url_origin1, net::SchemefulSite(), nullptr,
            blink::mojom::AncestorChainBit::kSameSite),
        StorageKey::CreateWithOptionalNonce(
            url_origin2, net::SchemefulSite(), nullptr,
            blink::mojom::AncestorChainBit::kSameSite),
    };

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
TEST(BlinkStorageKey, TopLevelSiteGetterWithPartitioningDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      net::features::kThirdPartyStoragePartitioning);
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

TEST(BlinkStorageKeyTest, NonceRequiresMatchingOriginSiteAndSameSite) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://foo.com");
  const BlinkSchemefulSite site(origin);
  const BlinkSchemefulSite opaque_site;
  const BlinkSchemefulSite other_site(
      SecurityOrigin::CreateFromString("https://notfoo.com"));
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // A nonce key with a matching origin/site that's SameSite works.
    std::ignore = BlinkStorageKey(origin, site, &nonce,
                                  mojom::blink::AncestorChainBit::kSameSite);

    // A nonce key with a non-matching origin/site that's SameSite fails.
    EXPECT_DCHECK_DEATH(
        BlinkStorageKey(origin, opaque_site, &nonce,
                        mojom::blink::AncestorChainBit::kSameSite));
    EXPECT_DCHECK_DEATH(BlinkStorageKey(
        origin, other_site, &nonce, mojom::blink::AncestorChainBit::kSameSite));

    // A nonce key with a matching origin/site that's CrossSite fails.
    EXPECT_DCHECK_DEATH(BlinkStorageKey(
        origin, site, &nonce, mojom::blink::AncestorChainBit::kCrossSite));

    // A nonce key with a non-matching origin/site that's CrossSite fails.
    EXPECT_DCHECK_DEATH(
        BlinkStorageKey(origin, opaque_site, &nonce,
                        mojom::blink::AncestorChainBit::kCrossSite));
    EXPECT_DCHECK_DEATH(
        BlinkStorageKey(origin, other_site, &nonce,
                        mojom::blink::AncestorChainBit::kCrossSite));
  }
}

TEST(BlinkStorageKeyTest, OpaqueTopLevelSiteRequiresSameSite) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://foo.com");
  const BlinkSchemefulSite site(origin);
  const BlinkSchemefulSite opaque_site;

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // A non-opaque site with SameSite and CrossSite works.
    std::ignore = BlinkStorageKey(origin, site, nullptr,
                                  mojom::blink::AncestorChainBit::kSameSite);
    std::ignore = BlinkStorageKey(origin, site, nullptr,
                                  mojom::blink::AncestorChainBit::kCrossSite);

    // An opaque site with SameSite works.
    std::ignore = BlinkStorageKey(origin, opaque_site, nullptr,
                                  mojom::blink::AncestorChainBit::kSameSite);

    // An opaque site with CrossSite fails.
    EXPECT_DCHECK_DEATH(
        BlinkStorageKey(origin, opaque_site, nullptr,
                        mojom::blink::AncestorChainBit::kCrossSite));
  }
}

TEST(BlinkStorageKeyTest, OriginAndSiteMismatchRequiresCrossSite) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://foo.com");
  scoped_refptr<const SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  const BlinkSchemefulSite site(origin);
  const BlinkSchemefulSite other_site(
      SecurityOrigin::CreateFromString("https://notfoo.com"));

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // A matching origin and site can be SameSite or CrossSite.
    std::ignore = BlinkStorageKey(origin, site, nullptr,
                                  mojom::blink::AncestorChainBit::kSameSite);
    std::ignore = BlinkStorageKey(origin, site, nullptr,
                                  mojom::blink::AncestorChainBit::kCrossSite);

    // A mismatched origin and site cannot be SameSite.
    EXPECT_DCHECK_DEATH(
        BlinkStorageKey(origin, other_site, nullptr,
                        mojom::blink::AncestorChainBit::kSameSite));
    EXPECT_DCHECK_DEATH(
        BlinkStorageKey(opaque_origin, other_site, nullptr,
                        mojom::blink::AncestorChainBit::kSameSite));

    // A mismatched origin and site must be CrossSite.
    std::ignore = BlinkStorageKey(origin, other_site, nullptr,
                                  mojom::blink::AncestorChainBit::kCrossSite);
  }
}

// Tests that FromWire() returns true/false correctly.
// If you make a change here, you should probably make it in StorageKeyTest too.
TEST(BlinkStorageKeyTest, FromWireReturnValue) {
  using AncestorChainBit = blink::mojom::AncestorChainBit;
  scoped_refptr<const SecurityOrigin> o1 =
      SecurityOrigin::CreateFromString("https://a.com");
  scoped_refptr<const SecurityOrigin> o2 =
      SecurityOrigin::CreateFromString("https://b.com");
  scoped_refptr<const SecurityOrigin> o3 =
      SecurityOrigin::CreateFromString("https://c.com");
  scoped_refptr<const SecurityOrigin> opaque =
      SecurityOrigin::CreateUniqueOpaque();
  const BlinkSchemefulSite site1 = BlinkSchemefulSite(o1);
  const BlinkSchemefulSite site2 = BlinkSchemefulSite(o2);
  const BlinkSchemefulSite site3 = BlinkSchemefulSite(o3);
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();

  const struct TestCase {
    scoped_refptr<const SecurityOrigin> origin;
    const BlinkSchemefulSite& top_level_site;
    const BlinkSchemefulSite& top_level_site_if_third_party_enabled;
    const absl::optional<base::UnguessableToken>& nonce;
    AncestorChainBit ancestor_chain_bit;
    AncestorChainBit ancestor_chain_bit_if_third_party_enabled;
    bool result;
  } test_cases[] = {
      // Passing cases:
      {o1, site1, site1, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, true},
      {o1, site1, site1, nonce1, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, true},
      {o1, site1, site2, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, true},
      {o1, site1, site1, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, true},
      {o1, site1, site1, nonce1, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, true},
      {opaque, site1, site1, absl::nullopt, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, true},
      // Failing cases:
      // If a 3p key is indicated, the *if_third_party_enabled pieces should
      // match their counterparts.
      {o1, site2, site3, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      {o1, site1, site1, absl::nullopt, AncestorChainBit::kCrossSite,
       AncestorChainBit::kSameSite, false},
      // If the top_level_site* is cross-site to the origin, the
      // ancestor_chain_bit* must indicate cross-site.
      {o1, site2, site2, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, false},
      {o1, site1, site2, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      {o1, site2, site2, absl::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      // If there is a nonce, all other values must indicate same-site to
      // origin.
      {o1, site2, site2, nonce1, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, false},
      {o1, site1, site1, nonce1, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, false},
      {o1, site1, site1, nonce1, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, false},
  };

  const BlinkStorageKey starting_key;

  for (const auto& test_case : test_cases) {
    BlinkStorageKey result_key = starting_key;
    EXPECT_EQ(
        test_case.result,
        BlinkStorageKey::FromWire(
            test_case.origin, test_case.top_level_site,
            test_case.top_level_site_if_third_party_enabled, test_case.nonce,
            test_case.ancestor_chain_bit,
            test_case.ancestor_chain_bit_if_third_party_enabled, result_key));
    if (!test_case.result) {
      // The key should not be modified for a return value of false.
      EXPECT_TRUE(starting_key.ExactMatchForTesting(result_key));
    }
  }
}
}  // namespace blink
