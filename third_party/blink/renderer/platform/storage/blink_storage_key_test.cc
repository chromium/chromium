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
  const BlinkStorageKey from_opaque =
      BlinkStorageKey::CreateFirstParty(std::move(opaque_origin));
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
    const BlinkStorageKey storage_key =
        BlinkStorageKey::CreateFirstParty(std::move(origin));
    EXPECT_TRUE(
        storage_key.GetSecurityOrigin()->IsSameOriginWith(copied.get()));

    // Test that two StorageKeys from the same origin are the same.
    const BlinkStorageKey storage_key_from_copy =
        BlinkStorageKey::CreateFirstParty(std::move(copied));
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
        BlinkStorageKey::CreateFirstParty(origin1),
        BlinkStorageKey::CreateFirstParty(origin2),
        BlinkStorageKey::CreateFirstParty(origin3),
        BlinkStorageKey::CreateFirstParty(origin4),
        BlinkStorageKey::CreateWithNonce(origin1, nonce),
        BlinkStorageKey::CreateWithNonce(origin2, nonce),
        BlinkStorageKey::Create(origin1, BlinkSchemefulSite(origin2),
                                mojom::blink::AncestorChainBit::kCrossSite),
        BlinkStorageKey::Create(origin1, BlinkSchemefulSite(),
                                mojom::blink::AncestorChainBit::kCrossSite),
        BlinkStorageKey::Create(origin2, BlinkSchemefulSite(),
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
TEST(BlinkStorageKeyTest, StorageKeyRoundTripConversion) {
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
        StorageKey::CreateFirstParty(url_origin1),
        StorageKey::CreateFirstParty(url_origin2),
        StorageKey::CreateFirstParty(url_origin3),
        StorageKey::CreateFirstParty(url_origin4),
        StorageKey::CreateWithNonce(url_origin1, nonce),
        StorageKey::CreateWithNonce(url_origin2, nonce),
        StorageKey::Create(url_origin1, net::SchemefulSite(url_origin2),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url_origin1, net::SchemefulSite(),
                           blink::mojom::AncestorChainBit::kCrossSite),
        StorageKey::Create(url_origin2, net::SchemefulSite(),
                           blink::mojom::AncestorChainBit::kCrossSite),
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
TEST(BlinkStorageKeyTest, CreateFromStringForTesting) {
  WTF::String example = "https://example.com/";
  WTF::String wrong = "I'm not a valid URL.";

  BlinkStorageKey key1 = BlinkStorageKey::CreateFromStringForTesting(example);
  BlinkStorageKey key2 = BlinkStorageKey::CreateFromStringForTesting(wrong);
  BlinkStorageKey key3 =
      BlinkStorageKey::CreateFromStringForTesting(WTF::String());

  EXPECT_FALSE(key1.GetSecurityOrigin()->IsOpaque());
  EXPECT_EQ(key1, BlinkStorageKey::CreateFirstParty(
                      SecurityOrigin::CreateFromString(example)));
  EXPECT_TRUE(key2.GetSecurityOrigin()->IsOpaque());
  EXPECT_TRUE(key3.GetSecurityOrigin()->IsOpaque());
}

// Test that BlinkStorageKey's top_level_site getter returns origin's site when
// storage partitioning is disabled.
TEST(BlinkStorageKeyTest, TopLevelSiteGetterWithPartitioningDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      net::features::kThirdPartyStoragePartitioning);
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key_origin1 = StorageKey::CreateFirstParty(origin1);
  StorageKey key_origin1_site1 =
      StorageKey::Create(origin1, net::SchemefulSite(origin1),
                         mojom::blink::AncestorChainBit::kSameSite);
  StorageKey key_origin1_site2 =
      StorageKey::Create(origin1, net::SchemefulSite(origin2),
                         mojom::blink::AncestorChainBit::kCrossSite);

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

  BlinkStorageKey key_origin1 = BlinkStorageKey::CreateFirstParty(origin1);
  BlinkStorageKey key_origin1_site1 =
      BlinkStorageKey::Create(origin1, BlinkSchemefulSite(origin1),
                              mojom::blink::AncestorChainBit::kSameSite);
  BlinkStorageKey key_origin1_site2 =
      BlinkStorageKey::Create(origin1, BlinkSchemefulSite(origin2),
                              mojom::blink::AncestorChainBit::kCrossSite);

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

    BlinkStorageKey storage_key =
        BlinkStorageKey::Create(origin1, BlinkSchemefulSite(origin2),
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

TEST(BlinkStorageKeyTest, NonceRequiresMatchingOriginSiteAndCrossSite) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://foo.com");
  scoped_refptr<const SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  const BlinkSchemefulSite site(origin);
  const BlinkSchemefulSite opaque_site(opaque_origin);
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // Test non-opaque origin.
    BlinkStorageKey key = BlinkStorageKey::CreateWithNonce(origin, nonce);
    EXPECT_EQ(key.GetAncestorChainBit(),
              mojom::blink::AncestorChainBit::kCrossSite);
    EXPECT_EQ(key.GetTopLevelSite(), site);

    // Test opaque origin.
    key = BlinkStorageKey::CreateWithNonce(opaque_origin, nonce);
    EXPECT_EQ(key.GetAncestorChainBit(),
              mojom::blink::AncestorChainBit::kCrossSite);
    EXPECT_EQ(key.GetTopLevelSite(), opaque_site);
  }
}

TEST(BlinkStorageKeyTest, OpaqueTopLevelSiteRequiresCrossSite) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://foo.com");
  const BlinkSchemefulSite site(origin);
  const BlinkSchemefulSite opaque_site;

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // A non-opaque site with SameSite and CrossSite works.
    std::ignore = BlinkStorageKey::Create(
        origin, site, mojom::blink::AncestorChainBit::kSameSite);
    std::ignore = BlinkStorageKey::Create(
        origin, site, mojom::blink::AncestorChainBit::kCrossSite);

    // An opaque site with CrossSite works.
    std::ignore = BlinkStorageKey::Create(
        origin, opaque_site, mojom::blink::AncestorChainBit::kCrossSite);

    // An opaque site with SameSite fails.
    EXPECT_DCHECK_DEATH(BlinkStorageKey::Create(
        origin, opaque_site, mojom::blink::AncestorChainBit::kSameSite));
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
    std::ignore = BlinkStorageKey::Create(
        origin, site, mojom::blink::AncestorChainBit::kSameSite);
    std::ignore = BlinkStorageKey::Create(
        origin, site, mojom::blink::AncestorChainBit::kCrossSite);

    // A mismatched origin and site cannot be SameSite.
    EXPECT_DCHECK_DEATH(BlinkStorageKey::Create(
        origin, other_site, mojom::blink::AncestorChainBit::kSameSite));
    EXPECT_DCHECK_DEATH(BlinkStorageKey::Create(
        opaque_origin, other_site, mojom::blink::AncestorChainBit::kSameSite));

    // A mismatched origin and site must be CrossSite.
    std::ignore = BlinkStorageKey::Create(
        origin, other_site, mojom::blink::AncestorChainBit::kCrossSite);
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
  const BlinkSchemefulSite opaque_site = BlinkSchemefulSite(opaque);
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();

  const struct TestCase {
    scoped_refptr<const SecurityOrigin> origin;
    const BlinkSchemefulSite top_level_site;
    const BlinkSchemefulSite top_level_site_if_third_party_enabled;
    const std::optional<base::UnguessableToken> nonce;
    AncestorChainBit ancestor_chain_bit;
    AncestorChainBit ancestor_chain_bit_if_third_party_enabled;
    bool result;
  } test_cases[] = {
      // Passing cases:
      {o1, site1, site1, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, true},
      {o1, site1, site1, nonce1, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, true},
      {o1, site1, site2, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, true},
      {o1, site1, site1, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, true},
      {o1, site1, site1, nonce1, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, true},
      {opaque, site1, site1, std::nullopt, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, true},
      {o1, site1, opaque_site, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, true},
      {o1, opaque_site, opaque_site, std::nullopt, AncestorChainBit::kCrossSite,
       AncestorChainBit::kCrossSite, true},
      {opaque, opaque_site, opaque_site, std::nullopt,
       AncestorChainBit::kCrossSite, AncestorChainBit::kCrossSite, true},
      // Failing cases:
      // If a 3p key is indicated, the *if_third_party_enabled pieces should
      // match their counterparts.
      {o1, site2, site3, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      {o1, site1, site1, std::nullopt, AncestorChainBit::kCrossSite,
       AncestorChainBit::kSameSite, false},
      // If the top_level_site* is cross-site to the origin, the
      // ancestor_chain_bit* must indicate cross-site.
      {o1, site2, site2, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, false},
      {o1, site1, site2, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      {o1, site2, site2, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      // If there is a nonce, all other values must indicate same-site to
      // origin.
      {o1, site2, site2, nonce1, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      {o1, site1, site1, nonce1, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      {o1, site1, site1, nonce1, AncestorChainBit::kSameSite,
       AncestorChainBit::kCrossSite, false},
      // If the top_level_site* is opaque, the ancestor_chain_bit* must be
      // same-site.
      {o1, site1, opaque_site, std::nullopt, AncestorChainBit::kCrossSite,
       AncestorChainBit::kSameSite, false},
      {o1, opaque_site, opaque_site, std::nullopt, AncestorChainBit::kSameSite,
       AncestorChainBit::kSameSite, false},
      // If the origin is opaque, the ancestor_chain_bit* must be cross-site.
      {opaque, opaque_site, opaque_site, std::nullopt,
       AncestorChainBit::kSameSite, AncestorChainBit::kSameSite, false},
      {opaque, opaque_site, opaque_site, std::nullopt,
       AncestorChainBit::kCrossSite, AncestorChainBit::kSameSite, false},
      {opaque, opaque_site, opaque_site, std::nullopt,
       AncestorChainBit::kSameSite, AncestorChainBit::kCrossSite, false},
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

TEST(BlinkStorageKeyTest, WithOrigin) {
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://foo.com");
  scoped_refptr<const SecurityOrigin> other_origin =
      SecurityOrigin::CreateFromString("https://notfoo.com");
  scoped_refptr<const SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  const BlinkSchemefulSite site(origin);
  const BlinkSchemefulSite other_site(other_origin);
  const BlinkSchemefulSite opaque_site(opaque_origin);
  const base::UnguessableToken nonce = base::UnguessableToken::Create();

  base::test::ScopedFeatureList scoped_feature_list;
  // WithOrigin's operation doesn't depend on the state of
  // kThirdPartyStoragePartitioning and toggling the feature's state makes the
  // test more difficult since the constructor's behavior *will* change. So we
  // only run with it on.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const struct {
    BlinkStorageKey original_key;
    scoped_refptr<const SecurityOrigin> new_origin;
    std::optional<BlinkStorageKey> expected_key;
  } kTestCases[] = {
      // No change in first-party key updated with same origin.
      {
          BlinkStorageKey::Create(origin, site,
                                  mojom::AncestorChainBit::kSameSite),
          origin,
          std::nullopt,
      },
      // Change in first-party key updated with new origin.
      {
          BlinkStorageKey::Create(origin, site,
                                  mojom::AncestorChainBit::kSameSite),
          other_origin,
          BlinkStorageKey::Create(other_origin, site,
                                  mojom::AncestorChainBit::kCrossSite),
      },
      // No change in third-party same-site key updated with same origin.
      {
          BlinkStorageKey::Create(origin, site,
                                  mojom::AncestorChainBit::kCrossSite),
          origin,
          std::nullopt,
      },
      // Change in third-party same-site key updated with same origin.
      {
          BlinkStorageKey::Create(origin, site,
                                  mojom::AncestorChainBit::kCrossSite),
          other_origin,
          BlinkStorageKey::Create(other_origin, site,
                                  mojom::AncestorChainBit::kCrossSite),
      },
      // No change in third-party key updated with same origin.
      {
          BlinkStorageKey::Create(origin, other_site,
                                  mojom::AncestorChainBit::kCrossSite),
          origin,
          std::nullopt,
      },
      // Change in third-party key updated with new origin.
      {
          BlinkStorageKey::Create(origin, other_site,
                                  mojom::AncestorChainBit::kCrossSite),
          other_origin,
          BlinkStorageKey::Create(other_origin, other_site,
                                  mojom::AncestorChainBit::kCrossSite),
      },
      // No change in opaque tls key updated with same origin.
      {
          BlinkStorageKey::Create(origin, opaque_site,
                                  mojom::AncestorChainBit::kCrossSite),
          origin,
          std::nullopt,
      },
      // Change in opaque tls key updated with new origin.
      {
          BlinkStorageKey::Create(origin, opaque_site,
                                  mojom::AncestorChainBit::kCrossSite),
          other_origin,
          BlinkStorageKey::Create(other_origin, opaque_site,
                                  mojom::AncestorChainBit::kCrossSite),
      },
      // No change in nonce key updated with same origin.
      {
          BlinkStorageKey::CreateWithNonce(origin, nonce),
          origin,
          std::nullopt,
      },
      // Change in nonce key updated with new origin.
      {
          BlinkStorageKey::CreateWithNonce(origin, nonce),
          other_origin,
          BlinkStorageKey::CreateWithNonce(other_origin, nonce),
      },
      // Change in opaque top_level_site key updated with opaque origin.
      {
          BlinkStorageKey::Create(origin, opaque_site,
                                  mojom::AncestorChainBit::kCrossSite),
          opaque_origin,
          BlinkStorageKey::Create(opaque_origin, opaque_site,
                                  mojom::AncestorChainBit::kCrossSite),
      },
  };

  for (const auto& test_case : kTestCases) {
    if (test_case.expected_key == std::nullopt) {
      EXPECT_EQ(test_case.original_key,
                test_case.original_key.WithOrigin(test_case.new_origin));
    } else {
      ASSERT_NE(test_case.expected_key, test_case.original_key);
      EXPECT_EQ(test_case.expected_key,
                test_case.original_key.WithOrigin(test_case.new_origin));
    }
  }
}
}  // namespace blink
