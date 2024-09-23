// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
namespace {

// Opaqueness here is used as a way of checking for "correctly constructed" in
// most tests.
//
// Why not call it IsValid()? Because some tests actually want to check for
// opaque origins.
bool IsOpaque(const StorageKey& key) {
  return key.origin().opaque() && key.top_level_site().opaque();
}

}  // namespace

class StorageKeyTest : public ::testing::Test {
 protected:
  const net::SchemefulSite GetOpaqueSite(uint64_t high,
                                         uint64_t low,
                                         std::string_view url_string) {
    return net::SchemefulSite(url::Origin(
        url::Origin::Nonce(base::UnguessableToken::CreateForTesting(high, low)),
        url::SchemeHostPort(GURL(url_string))));
  }

  std::vector<StorageKey> StorageKeysForCookiePartitionKeyTest(
      const base::UnguessableToken& nonce) {
    return std::vector<StorageKey>{
        /*Check storage key from string*/
        {StorageKey::CreateFromStringForTesting("https://www.example.com")},
        /*kCrossSite*/
        {StorageKey::Create(url::Origin::Create(GURL("https://www.foo.com")),
                            net::SchemefulSite(GURL("https://www.bar.com")),
                            mojom::AncestorChainBit::kCrossSite)},
        /*kSameSite keys check*/
        {StorageKey::Create(url::Origin::Create(GURL("https://www.foo.com")),
                            net::SchemefulSite(GURL("https://www.foo.com")),
                            mojom::AncestorChainBit::kSameSite)},
        /*First party check*/
        {StorageKey::CreateFirstParty(
            url::Origin::Create(GURL("https://www.foo.com")))},
        /*Nonced*/
        {StorageKey::CreateWithNonce(
            url::Origin::Create(GURL("https://www.example.com")), nonce)}};
  }
};

// Test when a constructed StorageKey object should be considered valid/opaque.
TEST_F(StorageKeyTest, ConstructionValidity) {
  StorageKey empty = StorageKey();
  EXPECT_TRUE(IsOpaque(empty));
  // These cases will have the same origin for both `origin` and
  // `top_level_site`.
  url::Origin valid_origin = url::Origin::Create(GURL("https://example.com"));
  const StorageKey valid = StorageKey::CreateFirstParty(valid_origin);
  EXPECT_FALSE(IsOpaque(valid));
  // Since the same origin is used for both `origin` and `top_level_site`, it is
  // by definition same-site.
  EXPECT_EQ(valid.ancestor_chain_bit(), mojom::AncestorChainBit::kSameSite);

  url::Origin invalid_origin =
      url::Origin::Create(GURL("I'm not a valid URL."));
  const StorageKey invalid = StorageKey::CreateFirstParty(invalid_origin);
  EXPECT_TRUE(IsOpaque(invalid));
}

// Test that StorageKeys are/aren't equivalent as expected when storage
// partitioning is disabled.
TEST_F(StorageKeyTest, EquivalenceUnpartitioned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      net::features::kThirdPartyStoragePartitioning);
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  url::Origin origin3 = url::Origin();
  // Create another opaque origin different from origin3.
  url::Origin origin4 = url::Origin();
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();

  // Ensure that the opaque origins produce opaque StorageKeys
  EXPECT_TRUE(IsOpaque(StorageKey::CreateFirstParty(origin3)));
  EXPECT_TRUE(IsOpaque(StorageKey::CreateFirstParty(origin4)));

  const struct {
    StorageKey storage_key1;
    StorageKey storage_key2;
    bool expected_equivalent;
  } kTestCases[] = {
      // StorageKeys made from the same origin are equivalent.
      {StorageKey::CreateFirstParty(origin1),
       StorageKey::CreateFirstParty(origin1), true},
      {StorageKey::CreateFirstParty(origin2),
       StorageKey::CreateFirstParty(origin2), true},
      {StorageKey::CreateFirstParty(origin3),
       StorageKey::CreateFirstParty(origin3), true},
      {StorageKey::CreateFirstParty(origin4),
       StorageKey::CreateFirstParty(origin4), true},
      // StorageKeys made from the same origin and nonce are equivalent.
      {StorageKey::CreateWithNonce(origin1, nonce1),
       StorageKey::CreateWithNonce(origin1, nonce1), true},
      {StorageKey::CreateWithNonce(origin1, nonce2),
       StorageKey::CreateWithNonce(origin1, nonce2), true},
      {StorageKey::CreateWithNonce(origin2, nonce1),
       StorageKey::CreateWithNonce(origin2, nonce1), true},
      // StorageKeys made from different origins are not equivalent.
      {StorageKey::CreateFirstParty(origin1),
       StorageKey::CreateFirstParty(origin2), false},
      {StorageKey::CreateFirstParty(origin3),
       StorageKey::CreateFirstParty(origin4), false},
      {StorageKey::CreateWithNonce(origin1, nonce1),
       StorageKey::CreateWithNonce(origin2, nonce1), false},
      // StorageKeys made from different nonces are not equivalent.
      {StorageKey::CreateWithNonce(origin1, nonce1),
       StorageKey::CreateWithNonce(origin1, nonce2), false},
      // StorageKeys made from different origins and nonce are not equivalent.
      {StorageKey::CreateWithNonce(origin1, nonce1),
       StorageKey::CreateWithNonce(origin2, nonce2), false},
      // When storage partitioning is disabled, the top-level site isn't taken
      // into account for equivalence.
      {StorageKey::Create(origin1, net::SchemefulSite(origin2),
                          blink::mojom::AncestorChainBit::kCrossSite),
       StorageKey::CreateFirstParty(origin1), true},
      {StorageKey::Create(origin2, net::SchemefulSite(origin1),
                          blink::mojom::AncestorChainBit::kCrossSite),
       StorageKey::CreateFirstParty(origin2), true},
  };
  for (const auto& test_case : kTestCases) {
    ASSERT_EQ(test_case.storage_key1 == test_case.storage_key2,
              test_case.expected_equivalent);
  }
}

// Test that StorageKeys are/aren't equivalent as expected when storage
// partitioning is enabled.
TEST_F(StorageKeyTest, EquivalencePartitioned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  net::SchemefulSite site1(origin1);
  net::SchemefulSite site2(origin2);

  // Keys should only match when both the origin and top-level site are the
  // same. Such as keys made from the single argument constructor and keys
  // created by the two argument constructor (when both arguments are the same
  // origin).

  const StorageKey OneArgKey_origin1 = StorageKey::CreateFirstParty(origin1);
  const StorageKey OneArgKey_origin2 = StorageKey::CreateFirstParty(origin2);

  StorageKey TwoArgKey_origin1_origin1 = StorageKey::Create(
      origin1, site1, blink::mojom::AncestorChainBit::kSameSite);
  StorageKey TwoArgKey_origin2_origin2 = StorageKey::Create(
      origin2, site2, blink::mojom::AncestorChainBit::kSameSite);

  EXPECT_EQ(OneArgKey_origin1, TwoArgKey_origin1_origin1);
  EXPECT_EQ(OneArgKey_origin2, TwoArgKey_origin2_origin2);

  // And when the two argument constructor gets different values
  StorageKey TwoArgKey1_origin1_origin2 = StorageKey::Create(
      origin1, site2, blink::mojom::AncestorChainBit::kCrossSite);
  StorageKey TwoArgKey2_origin1_origin2 = StorageKey::Create(
      origin1, site2, blink::mojom::AncestorChainBit::kCrossSite);
  StorageKey TwoArgKey_origin2_origin1 = StorageKey::Create(
      origin2, site1, blink::mojom::AncestorChainBit::kCrossSite);

  EXPECT_EQ(TwoArgKey1_origin1_origin2, TwoArgKey2_origin1_origin2);

  // Otherwise they're not equivalent.
  EXPECT_NE(TwoArgKey1_origin1_origin2, TwoArgKey_origin1_origin1);
  EXPECT_NE(TwoArgKey_origin2_origin1, TwoArgKey_origin2_origin2);
  EXPECT_NE(TwoArgKey1_origin1_origin2, TwoArgKey_origin2_origin1);
}

// Test that StorageKeys Serialize to the expected value with partitioning
// enabled and disabled.
TEST_F(StorageKeyTest, SerializeFirstParty) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    struct {
      const char* origin_str;
      const char* expected_serialization;
    } kTestCases[] = {
        {"https://example.com/", "https://example.com/"},
        // Trailing slash is added.
        {"https://example.com", "https://example.com/"},
        // Subdomains are preserved.
        {"http://sub.test.example/", "http://sub.test.example/"},
        // file: origins all serialize to "file:///"
        {"file:///", "file:///"},
        {"file:///foo/bar", "file:///"},
        {"file://example.fileshare.com/foo/bar", "file:///"},
    };

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_str);
      const StorageKey key =
          StorageKey::CreateFromStringForTesting(test.origin_str);
      EXPECT_EQ(test.expected_serialization, key.Serialize());
    }
  }
}

TEST_F(StorageKeyTest, SerializeFirstPartyForLocalStorage) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    struct {
      const char* origin_str;
      const char* expected_serialization;
    } kTestCases[] = {
        // Trailing slash is removed.
        {"https://example.com/", "https://example.com"},
        {"https://example.com", "https://example.com"},
        // Subdomains are preserved.
        {"http://sub.test.example/", "http://sub.test.example"},
        // file: origins all serialize to "file://"
        {"file://", "file://"},
        {"file:///foo/bar", "file://"},
        {"file://example.fileshare.com/foo/bar", "file://"},
    };

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_str);
      const StorageKey key =
          StorageKey::CreateFromStringForTesting(test.origin_str);
      EXPECT_EQ(test.expected_serialization, key.SerializeForLocalStorage());
    }
  }
}

// Tests that the top-level site is correctly serialized for service workers
// when kThirdPartyStoragePartitioning is enabled. This is expected to be the
// same for localStorage.
TEST_F(StorageKeyTest, SerializePartitioned) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  net::SchemefulSite SiteExample(GURL("https://example.com"));
  net::SchemefulSite SiteTest(GURL("https://test.example"));

  const struct {
    const char* origin;
    net::SchemefulSite top_level_site;
    mojom::AncestorChainBit ancestor_chain_bit;
    const char* expected_serialization;
  } kTestCases[] = {
      // 3p context cases
      {"https://example.com/", SiteTest, mojom::AncestorChainBit::kCrossSite,
       "https://example.com/^0https://test.example"},
      {"https://sub.test.example/", SiteExample,
       mojom::AncestorChainBit::kCrossSite,
       "https://sub.test.example/^0https://example.com"},
      {"https://example.com/", SiteExample, mojom::AncestorChainBit::kCrossSite,
       "https://example.com/^31"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    const url::Origin origin = url::Origin::Create(GURL(test.origin));
    StorageKey key = StorageKey::Create(origin, test.top_level_site,
                                        test.ancestor_chain_bit);
    EXPECT_EQ(test.expected_serialization, key.Serialize());
    EXPECT_EQ(test.expected_serialization, key.SerializeForLocalStorage());
  }
}

TEST_F(StorageKeyTest, SerializeNonce) {
  struct {
    const std::pair<const char*, const base::UnguessableToken> origin_and_nonce;
    const char* expected_serialization;
  } kTestCases[] = {
      {{"https://example.com/",
        base::UnguessableToken::CreateForTesting(12345ULL, 67890ULL)},
       "https://example.com/^112345^267890"},
      {{"https://test.example",
        base::UnguessableToken::CreateForTesting(22222ULL, 99999ULL)},
       "https://test.example/^122222^299999"},
      {{"https://sub.test.example/",
        base::UnguessableToken::CreateForTesting(9876ULL, 54321ULL)},
       "https://sub.test.example/^19876^254321"},
      {{"https://other.example/",
        base::UnguessableToken::CreateForTesting(3735928559ULL, 110521ULL)},
       "https://other.example/^13735928559^2110521"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_and_nonce.first);
    const url::Origin origin =
        url::Origin::Create(GURL(test.origin_and_nonce.first));
    const base::UnguessableToken& nonce = test.origin_and_nonce.second;
    StorageKey key = StorageKey::CreateWithNonce(origin, nonce);
    EXPECT_EQ(test.expected_serialization, key.Serialize());
  }
}

// Test that deserialized StorageKeys are valid/opaque as expected.
TEST_F(StorageKeyTest, Deserialize) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);
  const struct {
    std::string serialized_string;
    bool expected_has_value;
    bool expected_opaque = false;
  } kTestCases[] = {
      // Correct usage of origin.
      {"https://example.com/", true, false},
      // Correct usage of test.example origin.
      {"https://test.example/", true, false},
      // Correct usage of origin with port.
      {"https://example.com:90/", true, false},
      // Invalid origin URL.
      {"I'm not a valid URL.", false},
      // Empty string origin URL.
      {std::string(), false},
      // Correct usage of origin and top-level site.
      {"https://example.com/^0https://test.example", true, false},
      // Incorrect separator value used for top-level site.
      {"https://example.com/^1https://test.example", false},
      // Correct usage of origin and top-level site with test.example.
      {"https://test.example/^0https://example.com", true, false},
      // Invalid top-level site.
      {"https://example.com/^0I'm not a valid URL.", false},
      // Invalid origin with top-level site scheme.
      {"I'm not a valid URL.^0https://example.com", false},
      // Correct usage of origin and nonce.
      {"https://example.com/^112345^267890", true, false},
      // Nonce high not followed by nonce low.
      {"https://example.com/^112345^167890", false},
      // Nonce high not followed by nonce low; invalid separator value.
      {"https://example.com/^112345^967890", false},
      // Values encoded with nonce separator not a valid nonce.
      {"https://example.com/^1nota^2nonce", false},
      // Invalid origin with nonce scheme.
      {"I'm not a valid URL.^112345^267890", false},
      // Nonce low was incorrectly encoded before nonce high.
      {"https://example.com/^212345^167890", false},
      // Malformed usage of three separator carets.
      {"https://example.com/^112345^267890^", false},
      // Incorrect: Separator not followed by data.
      {"https://example.com/^1^267890", false},
      // Malformed first party serialization.
      {"https://www.example.com/^0https://example.com", false},
      // Malformed ancestor chain bit value - outside range.
      {"https://example.com/^35", false},
      // Correct ancestor chain bit value.
      {"https://example.com/^31", true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.serialized_string);
    std::optional<StorageKey> key =
        StorageKey::Deserialize(test_case.serialized_string);
    ASSERT_EQ(key.has_value(), test_case.expected_has_value);
    if (key.has_value())
      EXPECT_EQ(IsOpaque(*key), test_case.expected_opaque);
  }
}

// Test that string -> StorageKey test function performs as expected.
TEST_F(StorageKeyTest, CreateFromStringForTesting) {
  std::string example = "https://example.com/";
  std::string wrong = "I'm not a valid URL.";

  StorageKey key1 = StorageKey::CreateFromStringForTesting(example);
  StorageKey key2 = StorageKey::CreateFromStringForTesting(wrong);
  StorageKey key3 = StorageKey::CreateFromStringForTesting(std::string());

  EXPECT_FALSE(IsOpaque(key1));
  EXPECT_EQ(key1, StorageKey::CreateFromStringForTesting(example));
  EXPECT_TRUE(IsOpaque(key2));
  EXPECT_TRUE(IsOpaque(key3));
}

// Test that a StorageKey, constructed by deserializing another serialized
// StorageKey, is equivalent to the original.
TEST_F(StorageKeyTest, SerializeDeserialize) {
  const char* kTestCases[] = {"https://example.com", "https://sub.test.example",
                              "https://example.com:90", "file://",
                              "file://example.fileshare.com"};

  for (const char* test : kTestCases) {
    SCOPED_TRACE(test);
    url::Origin origin = url::Origin::Create(GURL(test));
    const StorageKey key = StorageKey::CreateFirstParty(origin);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    std::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    std::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::DeserializeForLocalStorage(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());
    if (origin.scheme() != "file") {
      EXPECT_EQ(key, *key_deserialized);
      EXPECT_EQ(key, *key_deserialized_from_local_storage);
    } else {
      // file origins are all collapsed to file:// by serialization.
      EXPECT_EQ(StorageKey::CreateFromStringForTesting("file://"),
                *key_deserialized);
      EXPECT_EQ(StorageKey::CreateFromStringForTesting("file://"),
                *key_deserialized_from_local_storage);
    }
  }
}

// Same as SerializeDeserialize but for partitioned StorageKeys when
// kThirdPartyStoragePartitioning is enabled.
TEST_F(StorageKeyTest, SerializeDeserializePartitioned) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  net::SchemefulSite SiteExample(GURL("https://example.com"));
  net::SchemefulSite SiteTest(GURL("https://test.example"));
  net::SchemefulSite SitePort(GURL("https://example.com:90"));
  net::SchemefulSite SiteFile(GURL("file:///"));

  const struct {
    const char* origin;
    net::SchemefulSite site;
  } kTestCases[] = {
      // 1p context case.
      {"https://example.com/", SiteExample},
      {"https://test.example", SiteTest},
      {"https://sub.test.example/", SiteTest},
      {"https://example.com:90", SitePort},
      // 3p context case
      {"https://example.com/", SiteTest},
      {"https://sub.test.example/", SiteExample},
      {"https://sub.test.example/", SitePort},
      // File case.
      {"file:///foo/bar", SiteFile},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    url::Origin origin = url::Origin::Create(GURL(test.origin));
    const net::SchemefulSite& site = test.site;

    StorageKey key =
        StorageKey::Create(origin, site,
                           net::SchemefulSite(origin) == site
                               ? mojom::AncestorChainBit::kSameSite
                               : mojom::AncestorChainBit::kCrossSite);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    std::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    std::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::DeserializeForLocalStorage(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());
    if (origin.scheme() != "file") {
      EXPECT_EQ(key, *key_deserialized);
      EXPECT_EQ(key, *key_deserialized_from_local_storage);
    } else {
      // file origins are all collapsed to file:// by serialization.
      EXPECT_EQ(StorageKey::Create(url::Origin::Create(GURL("file://")),
                                   net::SchemefulSite(GURL("file://")),
                                   mojom::AncestorChainBit::kSameSite),
                *key_deserialized);
      EXPECT_EQ(StorageKey::Create(url::Origin::Create(GURL("file://")),
                                   net::SchemefulSite(GURL("file://")),
                                   mojom::AncestorChainBit::kSameSite),
                *key_deserialized_from_local_storage);
    }
  }
}

TEST_F(StorageKeyTest, SerializeDeserializeNonce) {
  struct {
    const char* origin;
    const base::UnguessableToken nonce;
  } kTestCases[] = {
      {"https://example.com/",
       base::UnguessableToken::CreateForTesting(12345ULL, 67890ULL)},
      {"https://test.example",
       base::UnguessableToken::CreateForTesting(22222ULL, 99999ULL)},
      {"https://sub.test.example/",
       base::UnguessableToken::CreateForTesting(9876ULL, 54321ULL)},
      {"https://other.example/",
       base::UnguessableToken::CreateForTesting(3735928559ULL, 110521ULL)},
      {"https://other2.example/", base::UnguessableToken::Create()},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    url::Origin origin = url::Origin::Create(GURL(test.origin));
    const base::UnguessableToken& nonce = test.nonce;

    StorageKey key = StorageKey::CreateWithNonce(origin, nonce);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    std::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    std::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::DeserializeForLocalStorage(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());

    EXPECT_EQ(key, *key_deserialized);
    EXPECT_EQ(key, *key_deserialized_from_local_storage);
  }
}

TEST_F(StorageKeyTest, SerializeDeserializeOpaqueTopLevelSite) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    const struct {
      url::Origin origin;
      net::SchemefulSite top_level_site;
      const char* expected_serialization_without_partitioning;
      const char* expected_serialization_with_partitioning;
    } kTestCases[] = {
        {
            url::Origin::Create(GURL("https://example.com/")),
            GetOpaqueSite(12345ULL, 67890ULL, ""),
            "https://example.com/",
            "https://example.com/^412345^567890^6",
        },
        {
            url::Origin::Create(GURL("https://sub.example.com/")),
            GetOpaqueSite(22222ULL, 99999ULL, ""),
            "https://sub.example.com/",
            "https://sub.example.com/^422222^599999^6",
        },
        {
            url::Origin::Create(GURL("https://example.com/")),
            GetOpaqueSite(9876ULL, 54321ULL, "https://notexample.com/"),
            "https://example.com/",
            "https://example.com/^49876^554321^6https://notexample.com",
        },
        {
            url::Origin::Create(GURL("https://sub.example.com/")),
            GetOpaqueSite(3735928559ULL, 110521ULL,
                          "https://sub.notexample.com/"),
            "https://sub.example.com/",
            "https://sub.example.com/^43735928559^5110521^6https://"
            "sub.notexample.com",
        },
    };

    for (const auto& test : kTestCases) {
      if (toggle) {
        SCOPED_TRACE(test.expected_serialization_with_partitioning);
      } else {
        SCOPED_TRACE(test.expected_serialization_without_partitioning);
      }
      EXPECT_TRUE(test.top_level_site.opaque());
      StorageKey key =
          StorageKey::Create(test.origin, test.top_level_site,
                             blink::mojom::AncestorChainBit::kCrossSite);
      if (toggle) {
        EXPECT_EQ(test.expected_serialization_with_partitioning,
                  key.Serialize());
        EXPECT_EQ(key, StorageKey::Deserialize(
                           test.expected_serialization_with_partitioning));
      } else {
        EXPECT_EQ(test.expected_serialization_without_partitioning,
                  key.Serialize());
        EXPECT_EQ(key, StorageKey::Deserialize(
                           test.expected_serialization_without_partitioning));
      }
    }
  }
}

TEST_F(StorageKeyTest, DeserializeNonces) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    struct {
      const char* serialization;
      std::optional<blink::StorageKey> expected_key;
      const bool has_value_if_partitioning_is_disabled;
    } kTestCases[] = {
        {
            "https://example.com/^40^50^6",
            std::nullopt,
            false,
        },
        {
            "https://example.com/^41^50^6",
            blink::StorageKey::Create(
                url::Origin::Create(GURL("https://example.com/")),
                GetOpaqueSite(1ULL, 0ULL, ""),
                mojom::AncestorChainBit::kCrossSite),
            false,
        },
        {
            "https://example.com/^401^50^6",
            std::nullopt,
            false,
        },
        {
            "https://example.com/^40^51^6",
            blink::StorageKey::Create(
                url::Origin::Create(GURL("https://example.com/")),
                GetOpaqueSite(0ULL, 1ULL, ""),
                mojom::AncestorChainBit::kCrossSite),
            false,
        },
        {
            "https://example.com/^400^51^6",
            std::nullopt,
            false,
        },
        {
            "https://example.com/^41^51^6",
            blink::StorageKey::Create(
                url::Origin::Create(GURL("https://example.com/")),
                GetOpaqueSite(1ULL, 1ULL, ""),
                mojom::AncestorChainBit::kCrossSite),
            false,
        },
        {
            "https://example.com/^41^501^6",
            std::nullopt,
            false,
        },
        {
            "https://example.com/^10^20",
            std::nullopt,
            false,
        },
        {
            "https://example.com/^11^20",
            blink::StorageKey::CreateWithNonce(
                url::Origin::Create(GURL("https://example.com/")),
                base::UnguessableToken::CreateForTesting(1ULL, 0ULL)),
            true,
        },
        {
            "https://example.com/^101^20",
            std::nullopt,
            true,
        },
        {
            "https://example.com/^10^21",
            blink::StorageKey::CreateWithNonce(
                url::Origin::Create(GURL("https://example.com/")),
                base::UnguessableToken::CreateForTesting(0ULL, 1ULL)),
            true,
        },
        {
            "https://example.com/^100^21",
            std::nullopt,
            true,
        },
        {
            "https://example.com/^11^21",
            blink::StorageKey::CreateWithNonce(
                url::Origin::Create(GURL("https://example.com/")),
                base::UnguessableToken::CreateForTesting(1ULL, 1ULL)),
            true,
        },
        {
            "https://example.com/^11^201",
            std::nullopt,
            true,
        },
    };

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.serialization);
      std::optional<blink::StorageKey> maybe_storage_key =
          StorageKey::Deserialize(test.serialization);
      EXPECT_EQ((test.has_value_if_partitioning_is_disabled || toggle) &&
                    test.expected_key,
                (bool)maybe_storage_key);
      if (maybe_storage_key) {
        EXPECT_EQ(test.expected_key,
                  StorageKey::Deserialize(test.serialization));
      }
    }
  }
}

TEST_F(StorageKeyTest, DeserializeAncestorChainBits) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    struct {
      const char* serialization;
      std::optional<blink::StorageKey> expected_key;
    } kTestCases[] = {
        // An origin cannot be serialized with a SameSite bit.
        {
            "https://example.com/^30",
            std::nullopt,
        },
        // An origin cannot be serialized with a malformed CrossSite bit.
        {
            "https://example.com/^301",
            std::nullopt,
        },
        // An origin can be serialized with a CrossSite bit.
        {
            "https://example.com/^31",
            StorageKey::Create(url::Origin::Create(GURL("https://example.com")),
                               net::SchemefulSite(GURL("https://example.com")),
                               mojom::AncestorChainBit::kCrossSite),
        },
        // A mismatched origin and top_level_site cannot have a SameSite bit.
        {
            "https://example.com/^0https://notexample.com^30",
            std::nullopt,
        },
        // A mismatched origin and top_level_site cannot have a CrossSite bit.
        {
            "https://example.com/^0https://notexample.com^31",
            std::nullopt,
        },
        // A mismatched origin and top_level_site can have no bit.
        {
            "https://example.com/^0https://notexample.com",
            StorageKey::Create(
                url::Origin::Create(GURL("https://example.com")),
                net::SchemefulSite(GURL("https://notexample.com")),
                mojom::AncestorChainBit::kCrossSite),
        },
    };

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.serialization);
      if (toggle) {
        EXPECT_EQ(test.expected_key,
                  StorageKey::Deserialize(test.serialization));
      } else {
        EXPECT_FALSE(StorageKey::Deserialize(test.serialization));
      }
    }
  }
}

TEST_F(StorageKeyTest, IsThirdPartyStoragePartitioningEnabled) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    EXPECT_EQ(StorageKey::IsThirdPartyStoragePartitioningEnabled(), toggle);
  }
}

// Test that StorageKey's top_level_site getter returns origin's site when
// storage partitioning is disabled.
TEST_F(StorageKeyTest, TopLevelSiteGetterWithPartitioningDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      net::features::kThirdPartyStoragePartitioning);
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  net::SchemefulSite site1(origin1);
  net::SchemefulSite site2(origin2);

  const StorageKey key_origin1 = StorageKey::CreateFirstParty(origin1);
  StorageKey key_origin1_site1 =
      StorageKey::Create(origin1, site1, mojom::AncestorChainBit::kSameSite);
  StorageKey key_origin1_site2 =
      StorageKey::Create(origin1, site2, mojom::AncestorChainBit::kCrossSite);

  EXPECT_EQ(site1, key_origin1.top_level_site());
  EXPECT_EQ(site1, key_origin1_site1.top_level_site());
  EXPECT_EQ(site1, key_origin1_site2.top_level_site());
}

// Test that StorageKey's top_level_site getter returns the top level site when
// storage partitioning is enabled.
TEST_F(StorageKeyTest, TopLevelSiteGetterWithPartitioningEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  const StorageKey key_origin1 = StorageKey::CreateFirstParty(origin1);
  StorageKey key_origin1_site1 = StorageKey::Create(
      origin1, net::SchemefulSite(origin1), mojom::AncestorChainBit::kSameSite);
  StorageKey key_origin1_site2 =
      StorageKey::Create(origin1, net::SchemefulSite(origin2),
                         mojom::AncestorChainBit::kCrossSite);

  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin2), key_origin1_site2.top_level_site());
}

// Test that cross-origin keys cannot be deserialized when partitioning is
// disabled.
TEST_F(StorageKeyTest, AncestorChainBitGetterWithPartitioningDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      net::features::kThirdPartyStoragePartitioning);
  std::string cross_site_string = "https://example.com/^0https://test.example";
  std::optional<StorageKey> key_cross_site =
      StorageKey::Deserialize(cross_site_string);
  EXPECT_FALSE(key_cross_site.has_value());
}

// Test that the AncestorChainBit enum class is not reordered and returns the
// correct value when storage partitioning is enabled.
TEST_F(StorageKeyTest, AncestorChainBitGetterWithPartitioningEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);
  std::string cross_site_string = "https://example.com/^0https://test.example";
  std::optional<StorageKey> key_cross_site =
      StorageKey::Deserialize(cross_site_string);
  EXPECT_TRUE(key_cross_site.has_value());
  EXPECT_EQ(mojom::AncestorChainBit::kCrossSite,
            key_cross_site->ancestor_chain_bit());
}

TEST_F(StorageKeyTest, IsThirdPartyContext) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const url::Origin kOrigin = url::Origin::Create(GURL("https://www.foo.com"));
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://www.foo.com"));
  const url::Origin kSubdomainOrigin =
      url::Origin::Create(GURL("https://bar.foo.com"));
  const url::Origin kDifferentSite =
      url::Origin::Create(GURL("https://www.bar.com"));

  struct TestCase {
    const url::Origin origin;
    const url::Origin top_level_origin;
    const bool expected;
    const bool has_nonce = false;
  } test_cases[] = {{kOrigin, kOrigin, false},
                    {kOrigin, kInsecureOrigin, true},
                    {kOrigin, kSubdomainOrigin, false},
                    {kOrigin, kDifferentSite, true},
                    {kOrigin, kOrigin, true, true}};
  for (const auto& test_case : test_cases) {
    if (test_case.has_nonce) {
      StorageKey key = StorageKey::CreateWithNonce(
          test_case.origin, base::UnguessableToken::Create());
      EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
      EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
      continue;
    }
    mojom::AncestorChainBit ancestor_chain_bit =
        net::SchemefulSite(test_case.origin) ==
                net::SchemefulSite(test_case.top_level_origin)
            ? mojom::AncestorChainBit::kSameSite
            : mojom::AncestorChainBit::kCrossSite;
    StorageKey key = StorageKey::Create(
        test_case.origin, net::SchemefulSite(test_case.top_level_origin),
        ancestor_chain_bit);
    EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
    EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
    // IsThirdPartyContext should not depend on the order of the arguments.
    key = StorageKey::Create(test_case.top_level_origin,
                             net::SchemefulSite(test_case.origin),
                             ancestor_chain_bit);
    EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
    EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
  }
  // Explicitly testing the A->B->A case AncestorChainBit is preventing:
  // Same origin and top-level site but cross-site ancestor
  StorageKey cross_key =
      StorageKey::Create(kOrigin, net::SchemefulSite(kOrigin),
                         mojom::AncestorChainBit::kCrossSite);
  EXPECT_EQ(true, cross_key.IsThirdPartyContext());
}

TEST_F(StorageKeyTest, ToNetSiteForCookies) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const url::Origin kOrigin = url::Origin::Create(GURL("https://www.foo.com"));
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://www.foo.com"));
  const url::Origin kSubdomainOrigin =
      url::Origin::Create(GURL("https://bar.foo.com"));
  const url::Origin kDifferentSite =
      url::Origin::Create(GURL("https://www.bar.com"));

  struct TestCase {
    const url::Origin origin;
    const url::Origin top_level_origin;
    const net::SchemefulSite expected;
    const bool expected_opaque = false;
    const bool has_nonce = false;
  } test_cases[] = {
      {kOrigin, kOrigin, net::SchemefulSite(kOrigin)},
      {kOrigin, kInsecureOrigin, net::SchemefulSite(), true},
      {kInsecureOrigin, kOrigin, net::SchemefulSite(), true},
      {kOrigin, kSubdomainOrigin, net::SchemefulSite(kOrigin)},
      {kSubdomainOrigin, kOrigin, net::SchemefulSite(kOrigin)},
      {kOrigin, kDifferentSite, net::SchemefulSite(), true},
      {kOrigin, kOrigin, net::SchemefulSite(), true, true},
  };
  for (const auto& test_case : test_cases) {
    net::SchemefulSite got_site;
    if (test_case.has_nonce) {
      got_site = StorageKey::CreateWithNonce(test_case.origin,
                                             base::UnguessableToken::Create())
                     .ToNetSiteForCookies()
                     .site();
    } else {
      net::SchemefulSite top_level_site =
          net::SchemefulSite(test_case.top_level_origin);

      mojom::AncestorChainBit ancestor_chain_bit =
          top_level_site == net::SchemefulSite(test_case.origin)
              ? mojom::AncestorChainBit::kSameSite
              : mojom::AncestorChainBit::kCrossSite;

      got_site = StorageKey::Create(test_case.origin, top_level_site,
                                    ancestor_chain_bit)
                     .ToNetSiteForCookies()
                     .site();
    }
    if (test_case.expected_opaque) {
      EXPECT_TRUE(got_site.opaque());
      continue;
    }
    EXPECT_EQ(test_case.expected, got_site);
  }
}

TEST_F(StorageKeyTest, ToPartialNetIsolationInfo) {
  const auto kOrigin = url::Origin::Create(GURL("https://subdomain.foo.com"));
  const auto kOtherOrigin =
      url::Origin::Create(GURL("https://subdomain.bar.com"));
  const auto nonce = base::UnguessableToken::Create();

  {  // Same-site storage key
    const auto storage_key =
        StorageKey::Create(kOrigin, net::SchemefulSite(kOrigin),
                           mojom::AncestorChainBit::kSameSite);

    storage_key.ToPartialNetIsolationInfo().IsEqualForTesting(
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                   kOrigin, kOrigin,
                                   net::SiteForCookies::FromOrigin(kOrigin)));
  }

  {  // Cross-site storage key
    const auto storage_key =
        StorageKey::Create(kOrigin, net::SchemefulSite(kOtherOrigin),
                           mojom::AncestorChainBit::kCrossSite);

    storage_key.ToPartialNetIsolationInfo().IsEqualForTesting(
        net::IsolationInfo::Create(
            net::IsolationInfo::RequestType::kOther,
            net::SchemefulSite(kOrigin).GetInternalOriginForTesting(),
            kOtherOrigin, net::SiteForCookies()));
  }

  {  // Nonced key
    const auto storage_key = StorageKey::CreateWithNonce(kOrigin, nonce);

    storage_key.ToPartialNetIsolationInfo().IsEqualForTesting(
        net::IsolationInfo::Create(
            net::IsolationInfo::RequestType::kOther,
            net::SchemefulSite(kOrigin).GetInternalOriginForTesting(), kOrigin,
            net::SiteForCookies(), nonce));
  }
}

TEST_F(StorageKeyTest, CopyWithForceEnabledThirdPartyStoragePartitioning) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin kOtherOrigin = url::Origin::Create(GURL("https://bar.com"));

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    const StorageKey storage_key =
        StorageKey::Create(kOrigin, net::SchemefulSite(kOtherOrigin),
                           mojom::AncestorChainBit::kCrossSite);
    EXPECT_EQ(storage_key.IsThirdPartyContext(), toggle);
    EXPECT_EQ(storage_key.top_level_site(),
              net::SchemefulSite(toggle ? kOtherOrigin : kOrigin));
    EXPECT_EQ(storage_key.ancestor_chain_bit(),
              toggle ? mojom::AncestorChainBit::kCrossSite
                     : mojom::AncestorChainBit::kSameSite);

    const StorageKey storage_key_with_3psp =
        storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning();
    EXPECT_TRUE(storage_key_with_3psp.IsThirdPartyContext());
    EXPECT_EQ(storage_key_with_3psp.top_level_site(),
              net::SchemefulSite(kOtherOrigin));
    EXPECT_EQ(storage_key_with_3psp.ancestor_chain_bit(),
              mojom::AncestorChainBit::kCrossSite);
  }
}

// crbug.com/328043119 remove ToCookiePartitionKeyAncestorChainBitDisabled test
// when kAncestorChainBitEnabledInPartitionedCookies is no longer needed.
TEST_F(StorageKeyTest, ToCookiePartitionKeyAncestorChainBitDisabled) {
  auto nonce = base::UnguessableToken::Create();

  std::vector<StorageKey> storage_keys =
      StorageKeysForCookiePartitionKeyTest(nonce);

  // CookiePartitionKeys evaluate the state of the feature
  // kAncestorChainBitEnabledInPartitionedCookies during
  // object creation. The ScopedFeatureList is used here to ensure that the
  // CookiePartitionKeys created from the storage_keys vector have the expected
  // result in either state.

  {  // Ancestor Chain Bit disabled in Partitioned Cookies.
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatures(
        {net::features::kThirdPartyStoragePartitioning},
        {net::features::kAncestorChainBitEnabledInPartitionedCookies});

    std::vector<std::optional<net::CookiePartitionKey>> expected_cpk{
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.example.com"))},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://subdomain.bar.com"))},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.foo.com"),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.foo.com"),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.example.com"),
            net::CookiePartitionKey::AncestorChainBit::kCrossSite, nonce)},
    };

    std::vector<std::optional<net::CookiePartitionKey>> got;
    base::ranges::transform(
        storage_keys, std::back_inserter(got),
        [](const StorageKey& key) -> std::optional<net::CookiePartitionKey> {
          return key.ToCookiePartitionKey();
        });
    EXPECT_EQ(expected_cpk, got);
  }
}

TEST_F(StorageKeyTest, ToCookiePartitionKeyAncestorChainEnabled) {
  auto nonce = base::UnguessableToken::Create();

  std::vector<StorageKey> storage_keys =
      StorageKeysForCookiePartitionKeyTest(nonce);

  // CookiePartitionKeys evaluate the state of the feature
  // kAncestorChainBitEnabledInPartitionedCookies during
  // object creation. The ScopedFeatureList is used here to ensure that the
  // CookiePartitionKeys created from the storage_keys vector have the expected
  // result.

  {  // Ancestor Chain Bit enabled in Partitioned Cookies.
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatures(
        {net::features::kThirdPartyStoragePartitioning,
         net::features::kAncestorChainBitEnabledInPartitionedCookies},
        {});

    std::vector<std::optional<net::CookiePartitionKey>> expected_cpk{
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.example.com"),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://subdomain.bar.com"))},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.foo.com"),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.foo.com"),
            net::CookiePartitionKey::AncestorChainBit::kSameSite)},
        {net::CookiePartitionKey::FromURLForTesting(
            GURL("https://www.example.com"),
            net::CookiePartitionKey::AncestorChainBit::kCrossSite, nonce)},
    };
    std::vector<std::optional<net::CookiePartitionKey>> got;
    base::ranges::transform(
        storage_keys, std::back_inserter(got),
        [](const StorageKey& key) -> std::optional<net::CookiePartitionKey> {
          return key.ToCookiePartitionKey();
        });
    EXPECT_EQ(expected_cpk, got);
  }
}

TEST_F(StorageKeyTest, NonceRequiresMatchingOriginSiteAndCrossSite) {
  const url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin opaque_origin;
  const net::SchemefulSite site(origin);
  const net::SchemefulSite opaque_site(opaque_origin);
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // Test non-opaque origin.
    StorageKey key = StorageKey::CreateWithNonce(origin, nonce);
    EXPECT_EQ(key.ancestor_chain_bit(),
              blink::mojom::AncestorChainBit::kCrossSite);
    EXPECT_EQ(key.top_level_site(), site);

    // Test opaque origin.
    key = StorageKey::CreateWithNonce(opaque_origin, nonce);
    EXPECT_EQ(key.ancestor_chain_bit(),
              blink::mojom::AncestorChainBit::kCrossSite);
    EXPECT_EQ(key.top_level_site(), opaque_site);
  }
}

TEST_F(StorageKeyTest, OpaqueTopLevelSiteRequiresCrossSite) {
  const url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  const net::SchemefulSite site(origin);
  const net::SchemefulSite opaque_site;

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // A non-opaque site with SameSite and CrossSite works.
    std::ignore =
        StorageKey::Create(origin, site, mojom::AncestorChainBit::kSameSite);
    std::ignore =
        StorageKey::Create(origin, site, mojom::AncestorChainBit::kCrossSite);

    // An opaque site with CrossSite works.
    std::ignore = StorageKey::Create(origin, opaque_site,
                                     mojom::AncestorChainBit::kCrossSite);

    // An opaque site with SameSite fails.
    EXPECT_DCHECK_DEATH(StorageKey::Create(origin, opaque_site,
                                           mojom::AncestorChainBit::kSameSite));
  }
}

TEST_F(StorageKeyTest, ShouldSkipKeyDueToPartitioning) {
  const struct {
    std::string serialized_key;
    bool expected_skip;
  } kTestCases[] = {
      // First party keys should not be skipped.
      {
          "https://example.com/",
          false,
      },
      // Nonce keys should not be skipped.
      {
          "https://example.com/^19876^25432",
          false,
      },
      // Third-party keys should be skipped.
      {
          "https://example.com/^0https://test.example",
          true,
      },
      // Third-party keys should be skipped.
      {
          "https://example.com/^31",
          true,
      },
      // Third-party keys with opaque sites should be skipped.
      {
          "https://example.com/^412345^567890^6",
          true,
      },
      // Corrupted keys should be not skipped.
      {
          "https://example.com/^7",
          false,
      },
  };

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    for (const auto& test_case : kTestCases) {
      if (toggle) {
        EXPECT_FALSE(StorageKey::ShouldSkipKeyDueToPartitioning(
            test_case.serialized_key));
      } else {
        EXPECT_EQ(StorageKey::ShouldSkipKeyDueToPartitioning(
                      test_case.serialized_key),
                  test_case.expected_skip);
      }
    }
  }
}

TEST_F(StorageKeyTest, OriginAndSiteMismatchRequiresCrossSite) {
  const url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin opaque_origin;
  const net::SchemefulSite site(origin);
  const net::SchemefulSite other_site(GURL("https://notfoo.com"));

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // A matching origin and site can be SameSite or CrossSite.
    std::ignore =
        StorageKey::Create(origin, site, mojom::AncestorChainBit::kSameSite);
    std::ignore =
        StorageKey::Create(origin, site, mojom::AncestorChainBit::kCrossSite);

    // A mismatched origin and site cannot be SameSite.
    EXPECT_DCHECK_DEATH(StorageKey::Create(origin, other_site,
                                           mojom::AncestorChainBit::kSameSite));
    EXPECT_DCHECK_DEATH(StorageKey::Create(opaque_origin, other_site,
                                           mojom::AncestorChainBit::kSameSite));

    // A mismatched origin and site must be CrossSite.
    std::ignore = StorageKey::Create(origin, other_site,
                                     mojom::AncestorChainBit::kCrossSite);
  }
}

TEST_F(StorageKeyTest, WithOrigin) {
  const url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://notfoo.com"));
  const url::Origin opaque_origin;
  const net::SchemefulSite site(origin);
  const net::SchemefulSite other_site(other_origin);
  const net::SchemefulSite opaque_site(opaque_origin);
  const base::UnguessableToken nonce = base::UnguessableToken::Create();

  base::test::ScopedFeatureList scoped_feature_list;
  // WithOrigin's operation doesn't depend on the state of
  // kThirdPartyStoragePartitioning and toggling the feature's state makes the
  // test more difficult since Create()'s behavior *will*
  // change. So we only run with it on.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const struct {
    blink::StorageKey original_key;
    url::Origin new_origin;
    std::optional<blink::StorageKey> expected_key;
  } kTestCases[] = {
      // No change in first-party key updated with same origin.
      {
          blink::StorageKey::Create(origin, site,
                                    mojom::AncestorChainBit::kSameSite),
          origin,
          std::nullopt,
      },
      // Change in first-party key updated with new origin.
      {
          blink::StorageKey::Create(origin, site,
                                    mojom::AncestorChainBit::kSameSite),
          other_origin,
          blink::StorageKey::Create(other_origin, site,
                                    mojom::AncestorChainBit::kCrossSite),
      },
      // No change in third-party same-site key updated with same origin.
      {
          blink::StorageKey::Create(origin, site,
                                    mojom::AncestorChainBit::kCrossSite),
          origin,
          std::nullopt,
      },
      // Change in third-party same-site key updated with same origin.
      {
          blink::StorageKey::Create(origin, site,
                                    mojom::AncestorChainBit::kCrossSite),
          other_origin,
          blink::StorageKey::Create(other_origin, site,
                                    mojom::AncestorChainBit::kCrossSite),
      },
      // No change in third-party key updated with same origin.
      {
          blink::StorageKey::Create(origin, other_site,
                                    mojom::AncestorChainBit::kCrossSite),
          origin,
          std::nullopt,
      },
      // Change in third-party key updated with new origin.
      {
          blink::StorageKey::Create(origin, other_site,
                                    mojom::AncestorChainBit::kCrossSite),
          other_origin,
          blink::StorageKey::Create(other_origin, other_site,
                                    mojom::AncestorChainBit::kCrossSite),
      },
      // No change in opaque tls key updated with same origin.
      {
          blink::StorageKey::Create(origin, opaque_site,
                                    mojom::AncestorChainBit::kCrossSite),
          origin,
          std::nullopt,
      },
      // Change in opaque tls key updated with new origin.
      {
          blink::StorageKey::Create(origin, opaque_site,
                                    mojom::AncestorChainBit::kCrossSite),
          other_origin,
          blink::StorageKey::Create(other_origin, opaque_site,
                                    mojom::AncestorChainBit::kCrossSite),
      },
      // No change in nonce key updated with same origin.
      {
          blink::StorageKey::CreateWithNonce(origin, nonce),
          origin,
          std::nullopt,
      },
      // Change in nonce key updated with new origin.
      {
          blink::StorageKey::CreateWithNonce(origin, nonce),
          other_origin,
          blink::StorageKey::CreateWithNonce(other_origin, nonce),
      },
      // Change in opaque top_level_site key updated with opaque origin.
      {
          blink::StorageKey::Create(origin, opaque_site,
                                    mojom::AncestorChainBit::kCrossSite),
          opaque_origin,
          blink::StorageKey::Create(opaque_origin, opaque_site,
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

// Tests that FromWire() returns true/false correctly.
// If you make a change here, you should probably make it in BlinkStorageKeyTest
// too.
TEST_F(StorageKeyTest, FromWireReturnValue) {
  using AncestorChainBit = blink::mojom::AncestorChainBit;
  const url::Origin o1 = url::Origin::Create(GURL("https://a.com"));
  const url::Origin o2 = url::Origin::Create(GURL("https://b.com"));
  const url::Origin o3 = url::Origin::Create(GURL("https://c.com"));
  const url::Origin opaque = url::Origin();
  const net::SchemefulSite site1 = net::SchemefulSite(o1);
  const net::SchemefulSite site2 = net::SchemefulSite(o2);
  const net::SchemefulSite site3 = net::SchemefulSite(o3);
  const net::SchemefulSite opaque_site = net::SchemefulSite(opaque);
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();

  const struct TestCase {
    const url::Origin origin;
    const net::SchemefulSite top_level_site;
    const net::SchemefulSite top_level_site_if_third_party_enabled;
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
      // cross-site.
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

  const StorageKey starting_key;

  for (const auto& test_case : test_cases) {
    StorageKey result_key = starting_key;
    EXPECT_EQ(
        test_case.result,
        StorageKey::FromWire(
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

TEST_F(StorageKeyTest, CreateFromOriginAndIsolationInfo) {
  const url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://notfoo.com"));
  const url::Origin opaque_origin;
  const net::SchemefulSite site(origin);
  const net::SchemefulSite other_site(other_origin);
  const net::SchemefulSite opaque_site(opaque_origin);
  const base::UnguessableToken nonce = base::UnguessableToken::Create();

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    const struct {
      url::Origin new_origin;
      const net::IsolationInfo isolation_info;
      std::optional<blink::StorageKey> expected_key;
    } kTestCases[] = {
        // First party context.
        {
            origin,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, origin, origin,
                net::SiteForCookies::FromOrigin(origin),
                /*nonce=*/std::nullopt),
            blink::StorageKey::Create(origin, site,
                                      mojom::AncestorChainBit::kSameSite),
        },
        // Third party same-site context.
        {
            other_origin,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, other_origin,
                origin, net::SiteForCookies(),
                /*nonce=*/std::nullopt),
            blink::StorageKey::Create(other_origin, other_site,
                                      mojom::AncestorChainBit::kCrossSite),
        },
        // Third party context.
        {
            other_origin,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, origin, origin,
                net::SiteForCookies::FromOrigin(origin),
                /*nonce=*/std::nullopt),
            blink::StorageKey::Create(other_origin, site,
                                      mojom::AncestorChainBit::kCrossSite),
        },
        // Opaque TLS context.
        {
            origin,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, opaque_origin,
                opaque_origin, net::SiteForCookies::FromOrigin(opaque_origin),
                /*nonce=*/std::nullopt),
            blink::StorageKey::Create(origin, opaque_site,
                                      mojom::AncestorChainBit::kCrossSite),
        },
        // Nonce context.
        {
            origin,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, other_origin,
                other_origin, net::SiteForCookies::FromOrigin(other_origin),
                nonce),
            blink::StorageKey::CreateWithNonce(origin, nonce),
        },
        // Opaque context.
        {
            opaque_origin,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, origin, origin,
                net::SiteForCookies::FromOrigin(origin),
                /*nonce=*/std::nullopt),
            blink::StorageKey::Create(opaque_origin, site,
                                      mojom::AncestorChainBit::kCrossSite),
        },
    };

    for (const auto& test_case : kTestCases) {
      if (test_case.expected_key == std::nullopt) {
        EXPECT_DCHECK_DEATH(StorageKey::CreateFromOriginAndIsolationInfo(
            test_case.new_origin, test_case.isolation_info));
      } else {
        EXPECT_EQ(test_case.expected_key,
                  StorageKey::CreateFromOriginAndIsolationInfo(
                      test_case.new_origin, test_case.isolation_info));
      }
    }
  }
}

TEST_F(StorageKeyTest, MalformedOriginsAndSchemefulSites) {
  std::string kTestCases[] = {
      // We cannot omit the slash in a first party key.
      "https://example.com",
      // We cannot add a path in a first party key.
      "https://example.com/a",
      // We cannot omit the slash in a same-site third party key.
      "https://example.com^31",
      // We cannot add a path in a same-site third party key.
      "https://example.com/a^31",
      // We cannot omit the slash in a nonce key.
      "https://example.com^11^21",
      // We cannot add a path in a nonce key.
      "https://example.com/a^11^21",
      // We cannot omit the first slash in a third-party key.
      "https://example.com^0https://example.com",
      // We cannot add a final slash in a third-party key.
      "https://example.com/^0https://example.com/",
      // We cannot add a first path in a third-party key.
      "https://example.com/a^0https://example.com/",
      // We cannot add a final path in a third-party key.
      "https://example.com/^0https://example.com/a",
      // We cannot omit the slash in an opaque top level site key.
      "https://example.com^44^55^6",
      // We cannot add a path in an opaque top level site key.
      "https://example.com/a^44^55^6",
      // We cannot omit the first slash in an opaque precursor key.
      "https://example.com^44^55^6https://example.com",
      // We cannot add a final slash in an opaque precursor key.
      "https://example.com/^44^55^6https://example.com/",
      // We cannot add a first path in an opaque precursor key.
      "https://example.com/a^44^55^6https://example.com",
      // We cannot add a final path in an opaque precursor key.
      "https://example.com/^44^55^6https://example.com/a",
      // We cannot omit the slash in a first party file key.
      "file://",
      // We cannot add a path in a first party file key.
      "file:///a",
      // We cannot add a slash in a third party file key.
      "https://example.com/^0file:///",
      // We cannot add a path in a third party file key.
      "https://example.com/^0file:///a",
  };

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(test_case);
      EXPECT_FALSE(StorageKey::Deserialize(test_case));
    }
  }
}

TEST_F(StorageKeyTest, DeserializeForLocalStorageFirstParty) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // This should deserialize as it lacks a trailing slash.
    EXPECT_TRUE(StorageKey::DeserializeForLocalStorage("https://example.com")
                    .has_value());

    // This should deserialize as it lacks a trailing slash.
    EXPECT_FALSE(StorageKey::DeserializeForLocalStorage("https://example.com/")
                     .has_value());
  }
}

TEST_F(StorageKeyTest,
       SerializeDeserializeWithAndWithoutThirdPartyStoragePartitioning) {
  struct {
    const std::string serialized_key;
    const bool has_value_if_partitioning_is_disabled;
  } kTestCases[] = {
      // This is a valid first-party file key.
      {
          "file:///",
          true,
      },
      // This is a valid third-party file key.
      {
          "file:///^31",
          false,
      },
      // This is a valid first-party origin key.
      {
          "https://example.com/",
          true,
      },
      // This is a valid third-party origin key.
      {
          "https://example.com/^31",
          false,
      },
      // This is a valid third-party cross-origin key.
      {
          "https://example.com/^0https://notexample.com",
          false,
      },
      // This is a valid nonce key.
      {
          "https://example.com/^11^21",
          true,
      },
      // This is a valid opaque top_level_site key.
      {
          "https://example.com/^41^51^6",
          false,
      },
  };

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    for (const auto& test_case : kTestCases) {
      const std::optional<blink::StorageKey> maybe_storage_key =
          StorageKey::Deserialize(test_case.serialized_key);
      EXPECT_EQ(test_case.has_value_if_partitioning_is_disabled || toggle,
                (bool)maybe_storage_key);
      if (maybe_storage_key) {
        EXPECT_EQ(test_case.serialized_key, maybe_storage_key->Serialize());
      }
    }
  }
}
}  // namespace blink
