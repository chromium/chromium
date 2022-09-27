// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <utility>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

// Test when a constructed StorageKey object should be considered valid/opaque.
TEST(StorageKeyTest, ConstructionValidity) {
  StorageKey empty = StorageKey();
  EXPECT_TRUE(IsOpaque(empty));
  // These cases will have the same origin for both `origin` and
  // `top_level_site`.
  url::Origin valid_origin = url::Origin::Create(GURL("https://example.com"));
  StorageKey valid = StorageKey(valid_origin);
  EXPECT_FALSE(IsOpaque(valid));
  // Since the same origin is used for both `origin` and `top_level_site`, it is
  // by definition same-site.
  EXPECT_EQ(valid.ancestor_chain_bit(), mojom::AncestorChainBit::kSameSite);

  url::Origin invalid_origin =
      url::Origin::Create(GURL("I'm not a valid URL."));
  StorageKey invalid = StorageKey(invalid_origin);
  EXPECT_TRUE(IsOpaque(invalid));
}

// Test that StorageKeys are/aren't equivalent as expected when storage
// partitioning is disabled.
TEST(StorageKeyTest, Equivalence) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  url::Origin origin3 = url::Origin();
  // Create another opaque origin different from origin3.
  url::Origin origin4 = url::Origin();
  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();

  // Ensure that the opaque origins produce opaque StorageKeys
  EXPECT_TRUE(IsOpaque(StorageKey(origin3)));
  EXPECT_TRUE(IsOpaque(StorageKey(origin4)));

  const struct {
    StorageKey storage_key1;
    StorageKey storage_key2;
    bool expected_equivalent;
  } kTestCases[] = {
      // StorageKeys made from the same origin are equivalent.
      {StorageKey(origin1), StorageKey(origin1), true},
      {StorageKey(origin2), StorageKey(origin2), true},
      {StorageKey(origin3), StorageKey(origin3), true},
      {StorageKey(origin4), StorageKey(origin4), true},
      // StorageKeys made from the same origin and nonce are equivalent.
      {StorageKey::CreateWithNonce(origin1, nonce1),
       StorageKey::CreateWithNonce(origin1, nonce1), true},
      {StorageKey::CreateWithNonce(origin1, nonce2),
       StorageKey::CreateWithNonce(origin1, nonce2), true},
      {StorageKey::CreateWithNonce(origin2, nonce1),
       StorageKey::CreateWithNonce(origin2, nonce1), true},
      // StorageKeys made from different origins are not equivalent.
      {StorageKey(origin1), StorageKey(origin2), false},
      {StorageKey(origin3), StorageKey(origin4), false},
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
      {StorageKey::CreateForTesting(origin1, origin2), StorageKey(origin1),
       true},
      {StorageKey::CreateForTesting(origin2, origin1), StorageKey(origin2),
       true},
  };
  for (const auto& test_case : kTestCases) {
    ASSERT_EQ(test_case.storage_key1 == test_case.storage_key2,
              test_case.expected_equivalent);
  }
}

// Test that StorageKeys are/aren't equivalent as expected when storage
// partitioning is enabled.
TEST(StorageKeyTest, EquivalencePartitioned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  // Keys should only match when both the origin and top-level site are the
  // same. Such as keys made from the single argument constructor and keys
  // created by the two argument constructor (when both arguments are the same
  // origin).

  StorageKey OneArgKey_origin1 = StorageKey(origin1);
  StorageKey OneArgKey_origin2 = StorageKey(origin2);

  StorageKey TwoArgKey_origin1_origin1 =
      StorageKey::CreateForTesting(origin1, origin1);
  StorageKey TwoArgKey_origin2_origin2 =
      StorageKey::CreateForTesting(origin2, origin2);

  EXPECT_EQ(OneArgKey_origin1, TwoArgKey_origin1_origin1);
  EXPECT_EQ(OneArgKey_origin2, TwoArgKey_origin2_origin2);

  // And when the two argument constructor gets different values
  StorageKey TwoArgKey1_origin1_origin2 =
      StorageKey::CreateForTesting(origin1, origin2);
  StorageKey TwoArgKey2_origin1_origin2 =
      StorageKey::CreateForTesting(origin1, origin2);
  StorageKey TwoArgKey_origin2_origin1 =
      StorageKey::CreateForTesting(origin2, origin1);

  EXPECT_EQ(TwoArgKey1_origin1_origin2, TwoArgKey2_origin1_origin2);

  // Otherwise they're not equivalent.
  EXPECT_NE(TwoArgKey1_origin1_origin2, TwoArgKey_origin1_origin1);
  EXPECT_NE(TwoArgKey_origin2_origin1, TwoArgKey_origin2_origin2);
  EXPECT_NE(TwoArgKey1_origin1_origin2, TwoArgKey_origin2_origin1);
}

// Test that StorageKeys Serialize to the expected value with partitioning
// enabled and disabled.
TEST(StorageKeyTest, SerializeFirstParty) {
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
      StorageKey key(url::Origin::Create(GURL(test.origin_str)));
      EXPECT_EQ(test.expected_serialization, key.Serialize());
    }
  }
}

TEST(StorageKeyTest, SerializeFirstPartyForLocalStorage) {
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
      StorageKey key(url::Origin::Create(GURL(test.origin_str)));
      EXPECT_EQ(test.expected_serialization, key.SerializeForLocalStorage());
    }
  }
}

// Tests that the top-level site is correctly serialized for service workers
// when kThirdPartyStoragePartitioning is enabled. This is expected to be the
// same for localStorage.
TEST(StorageKeyTest, SerializePartitioned) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  net::SchemefulSite SiteExample(GURL("https://example.com"));
  net::SchemefulSite SiteTest(GURL("https://test.example"));

  struct {
    const char* origin;
    const net::SchemefulSite& top_level_site;
    const mojom::AncestorChainBit ancestor_chain_bit;
    const char* expected_serialization;
  } kTestCases[] = {
      // 3p context cases
      {"https://example.com/", SiteTest, mojom::AncestorChainBit::kCrossSite,
       "https://example.com/^0https://test.example^31"},
      {"https://sub.test.example/", SiteExample,
       mojom::AncestorChainBit::kCrossSite,
       "https://sub.test.example/^0https://example.com^31"},
      {"https://example.com/", SiteExample, mojom::AncestorChainBit::kCrossSite,
       "https://example.com/^0https://example.com^31"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    const url::Origin origin = url::Origin::Create(GURL(test.origin));
    const net::SchemefulSite& site = test.top_level_site;
    StorageKey key = StorageKey::CreateWithOptionalNonce(
        origin, site, nullptr, test.ancestor_chain_bit);
    EXPECT_EQ(test.expected_serialization, key.Serialize());
    EXPECT_EQ(test.expected_serialization, key.SerializeForLocalStorage());
  }
}

TEST(StorageKeyTest, SerializeNonce) {
  struct {
    const std::pair<const char*, const base::UnguessableToken> origin_and_nonce;
    const char* expected_serialization;
  } kTestCases[] = {
      {{"https://example.com/",
        base::UnguessableToken::Deserialize(12345ULL, 67890ULL)},
       "https://example.com/^112345^267890"},
      {{"https://test.example",
        base::UnguessableToken::Deserialize(22222ULL, 99999ULL)},
       "https://test.example/^122222^299999"},
      {{"https://sub.test.example/",
        base::UnguessableToken::Deserialize(9876ULL, 54321ULL)},
       "https://sub.test.example/^19876^254321"},
      {{"https://other.example/",
        base::UnguessableToken::Deserialize(3735928559ULL, 110521ULL)},
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
TEST(StorageKeyTest, Deserialize) {
  const struct {
    std::string serialized_string;
    bool expected_has_value;
    bool expected_opaque = false;
  } kTestCases[] = {
      // Correct usage of origin.
      {"https://example.com/", true, false},
      // Correct: localstorage serialization doesn't have a trailing slash.
      {"https://example.com", true, false},
      // Correct usage of test.example origin.
      {"https://test.example/", true, false},
      // Invalid origin URL.
      {"I'm not a valid URL.", false},
      // Empty string origin URL.
      {std::string(), false},
      // Correct usage of origin and top-level site.
      {"https://example.com/^0https://test.example^31", true, false},
      // Incorrect separator value used for top-level site.
      {"https://example.com/^1https://test.example^31", false},
      // Correct usage of origin and top-level site with test.example.
      {"https://test.example/^0https://example.com^31", true, false},
      // Invalid top-level site.
      {"https://example.com/^0I'm not a valid URL.^31", false},
      // Invalid origin with top-level site scheme.
      {"I'm not a valid URL.^0https://example.com^31", false},
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
      {"https://www.example.com/^0https://example.com^30", false},
      // Malformed ancestor chain bit value - outside range.
      {"https://example.com^0https://test.example^35", false},
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<StorageKey> key =
        StorageKey::Deserialize(test_case.serialized_string);
    ASSERT_EQ(key.has_value(), test_case.expected_has_value);
    if (key.has_value())
      EXPECT_EQ(IsOpaque(*key), test_case.expected_opaque);
  }
}

// Test that string -> StorageKey test function performs as expected.
TEST(StorageKeyTest, CreateFromStringForTesting) {
  std::string example = "https://example.com/";
  std::string wrong = "I'm not a valid URL.";

  StorageKey key1 = StorageKey::CreateFromStringForTesting(example);
  StorageKey key2 = StorageKey::CreateFromStringForTesting(wrong);
  StorageKey key3 = StorageKey::CreateFromStringForTesting(std::string());

  EXPECT_FALSE(IsOpaque(key1));
  EXPECT_EQ(key1, StorageKey(url::Origin::Create(GURL(example))));
  EXPECT_TRUE(IsOpaque(key2));
  EXPECT_TRUE(IsOpaque(key3));
}

// Test that a StorageKey, constructed by deserializing another serialized
// StorageKey, is equivalent to the original.
TEST(StorageKeyTest, SerializeDeserialize) {
  const char* kTestCases[] = {"https://example.com", "https://sub.test.example",
                              "file://", "file://example.fileshare.com"};

  for (const char* test : kTestCases) {
    SCOPED_TRACE(test);
    url::Origin origin = url::Origin::Create(GURL(test));
    StorageKey key(origin);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    absl::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    absl::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::Deserialize(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());
    if (origin.scheme() != "file") {
      EXPECT_EQ(key, *key_deserialized);
      EXPECT_EQ(key, *key_deserialized_from_local_storage);
    } else {
      // file origins are all collapsed to file:// by serialization.
      EXPECT_EQ(StorageKey(url::Origin::Create(GURL("file://"))),
                *key_deserialized);
      EXPECT_EQ(StorageKey(url::Origin::Create(GURL("file://"))),
                *key_deserialized_from_local_storage);
    }
  }
}

// Same as SerializeDeserialize but for partitioned StorageKeys when
// kThirdPartyStoragePartitioning is enabled.
TEST(StorageKeyTest, SerializeDeserializePartitioned) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  net::SchemefulSite SiteExample(GURL("https://example.com"));
  net::SchemefulSite SiteTest(GURL("https://test.example"));
  net::SchemefulSite SiteFile(GURL("file:///"));

  struct {
    const char* origin;
    const net::SchemefulSite& site;
  } kTestCases[] = {
      // 1p context case.
      {"https://example.com/", SiteExample},
      {"https://test.example", SiteTest},
      {"https://sub.test.example/", SiteTest},
      // 3p context case
      {"https://example.com/", SiteTest},
      {"https://sub.test.example/", SiteExample},
      // File case.
      {"file:///foo/bar", SiteFile},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    url::Origin origin = url::Origin::Create(GURL(test.origin));
    const net::SchemefulSite& site = test.site;

    StorageKey key = StorageKey::CreateForTesting(origin, site);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    absl::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    absl::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::Deserialize(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());
    if (origin.scheme() != "file") {
      EXPECT_EQ(key, *key_deserialized);
      EXPECT_EQ(key, *key_deserialized_from_local_storage);
    } else {
      // file origins are all collapsed to file:// by serialization.
      EXPECT_EQ(
          StorageKey::CreateForTesting(url::Origin::Create(GURL("file://")),
                                       net::SchemefulSite(GURL("file://"))),
          *key_deserialized);
      EXPECT_EQ(
          StorageKey::CreateForTesting(url::Origin::Create(GURL("file://")),
                                       net::SchemefulSite(GURL("file://"))),
          *key_deserialized_from_local_storage);
    }
  }
}

TEST(StorageKeyTest, SerializeDeserializeNonce) {
  struct {
    const char* origin;
    const base::UnguessableToken nonce;
  } kTestCases[] = {
      {"https://example.com/",
       base::UnguessableToken::Deserialize(12345ULL, 67890ULL)},
      {"https://test.example",
       base::UnguessableToken::Deserialize(22222ULL, 99999ULL)},
      {"https://sub.test.example/",
       base::UnguessableToken::Deserialize(9876ULL, 54321ULL)},
      {"https://other.example/",
       base::UnguessableToken::Deserialize(3735928559ULL, 110521ULL)},
      {"https://other2.example/", base::UnguessableToken::Create()},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    url::Origin origin = url::Origin::Create(GURL(test.origin));
    const base::UnguessableToken& nonce = test.nonce;

    StorageKey key = StorageKey::CreateWithNonce(origin, nonce);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    absl::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    absl::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::Deserialize(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());

    EXPECT_EQ(key, *key_deserialized);
    EXPECT_EQ(key, *key_deserialized_from_local_storage);
  }
}

TEST(StorageKeyTest, IsThirdPartyStoragePartitioningEnabled) {
  EXPECT_FALSE(StorageKey::IsThirdPartyStoragePartitioningEnabled());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);
  EXPECT_TRUE(StorageKey::IsThirdPartyStoragePartitioningEnabled());
}

// Test that StorageKey's top_level_site getter returns origin's site when
// storage partitioning is disabled.
TEST(StorageKeyTest, TopLevelSiteGetter) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key_origin1 = StorageKey(origin1);
  StorageKey key_origin1_site1 = StorageKey::CreateForTesting(origin1, origin1);
  StorageKey key_origin1_site2 = StorageKey::CreateForTesting(origin1, origin2);

  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site2.top_level_site());
}

// Test that StorageKey's top_level_site getter returns the top level site when
// storage partitioning is enabled.
TEST(StorageKeyTest, TopLevelSiteGetterWithPartitioningEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key_origin1 = StorageKey(origin1);
  StorageKey key_origin1_site1 = StorageKey::CreateForTesting(origin1, origin1);
  StorageKey key_origin1_site2 = StorageKey::CreateForTesting(origin1, origin2);

  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin2), key_origin1_site2.top_level_site());
}

// Test that the AncestorChainBit enum class is not reordered and returns
// kSameSite when partitioning is not enabled.
TEST(StorageKeyTest, AncestorChainBitGetter) {
  std::string same_site_string =
      "https://example.com/^0https://test.example^30";
  std::string cross_site_string =
      "https://example.com/^0https://test.example^31";

  absl::optional<StorageKey> key_same_site =
      StorageKey::Deserialize(same_site_string);
  absl::optional<StorageKey> key_cross_site =
      StorageKey::Deserialize(cross_site_string);

  EXPECT_TRUE(key_same_site.has_value());
  EXPECT_TRUE(key_cross_site.has_value());
  EXPECT_EQ(mojom::AncestorChainBit::kSameSite,
            key_same_site->ancestor_chain_bit());
  EXPECT_EQ(mojom::AncestorChainBit::kSameSite,
            key_cross_site->ancestor_chain_bit());
}

// Test that the AncestorChainBit enum class is not reordered and returns the
// correct value when storage partitioning is enabled.
TEST(StorageKeyTest, AncestorChainBitGetterWithPartitioningEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);
  std::string same_site_string =
      "https://example.com/^0https://test.example^30";
  std::string cross_site_string =
      "https://example.com/^0https://test.example^31";

  absl::optional<StorageKey> key_same_site =
      StorageKey::Deserialize(same_site_string);
  absl::optional<StorageKey> key_cross_site =
      StorageKey::Deserialize(cross_site_string);

  EXPECT_TRUE(key_same_site.has_value());
  EXPECT_TRUE(key_cross_site.has_value());
  EXPECT_EQ(mojom::AncestorChainBit::kSameSite,
            key_same_site->ancestor_chain_bit());
  EXPECT_EQ(mojom::AncestorChainBit::kCrossSite,
            key_cross_site->ancestor_chain_bit());
}

TEST(StorageKeyTest, IsThirdPartyContext) {
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
    StorageKey key = StorageKey::CreateForTesting(test_case.origin,
                                                  test_case.top_level_origin);
    EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
    EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
    // IsThirdPartyContext should not depend on the order of the arguments.
    key = StorageKey::CreateForTesting(test_case.top_level_origin,
                                       test_case.origin);
    EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
    EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
  }
  // Explicitly testing the A->B->A case AncestorChainBit is preventing:
  // Same origin and top-level site but cross-site ancestor
  StorageKey cross_key = StorageKey::CreateWithOptionalNonce(
      kOrigin, net::SchemefulSite(kOrigin), nullptr,
      mojom::AncestorChainBit::kCrossSite);
  EXPECT_EQ(true, cross_key.IsThirdPartyContext());
}

TEST(StorageKeyTest, ToNetSiteForCookies) {
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

      got_site =
          StorageKey::CreateWithOptionalNonce(test_case.origin, top_level_site,
                                              nullptr, ancestor_chain_bit)
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

TEST(StorageKeyTest, CopyWithForceEnabledThirdPartyStoragePartitioning) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin kOtherOrigin = url::Origin::Create(GURL("https://bar.com"));

  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    const StorageKey storage_key = StorageKey::CreateWithOptionalNonce(
        kOrigin, net::SchemefulSite(kOtherOrigin), nullptr,
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

TEST(StorageKeyTest, ToCookiePartitionKey) {
  struct TestCase {
    const StorageKey storage_key;
    const absl::optional<net::CookiePartitionKey> expected;
  };

  auto nonce = base::UnguessableToken::Create();

  {  // Cookie partitioning disabled.
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatures(
        {net::features::kThirdPartyStoragePartitioning},
        {net::features::kPartitionedCookies,
         net::features::kNoncedPartitionedCookies});

    TestCase test_cases[] = {
        {StorageKey(url::Origin::Create(GURL("https://www.example.com"))),
         absl::nullopt},
        {StorageKey::CreateForTesting(
             url::Origin::Create(GURL("https://www.foo.com")),
             url::Origin::Create(GURL("https://www.bar.com"))),
         absl::nullopt},
        {StorageKey::CreateWithNonce(
             url::Origin::Create(GURL("https://www.example.com")), nonce),
         absl::nullopt},
    };
    for (const auto& test_case : test_cases) {
      EXPECT_EQ(test_case.expected,
                test_case.storage_key.ToCookiePartitionKey());
    }
  }

  {
    // Nonced partitioned cookies enabled only.
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatures(
        {net::features::kThirdPartyStoragePartitioning,
         net::features::kNoncedPartitionedCookies},
        {net::features::kPartitionedCookies});

    TestCase test_cases[] = {
        {StorageKey(url::Origin::Create(GURL("https://www.example.com"))),
         absl::nullopt},
        {StorageKey::CreateWithNonce(
             url::Origin::Create(GURL("https://www.example.com")), nonce),
         net::CookiePartitionKey::FromURLForTesting(GURL("https://example.com"),
                                                    nonce)},
    };
    for (const auto& test_case : test_cases) {
      EXPECT_EQ(test_case.expected,
                test_case.storage_key.ToCookiePartitionKey());
    }
  }

  {  // Cookie partitioning enabled.
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatures(
        {net::features::kThirdPartyStoragePartitioning,
         net::features::kPartitionedCookies},
        {});

    TestCase test_cases[] = {
        {StorageKey(url::Origin::Create(GURL("https://www.example.com"))),
         net::CookiePartitionKey::FromURLForTesting(
             GURL("https://www.example.com"))},
        {StorageKey::CreateForTesting(
             url::Origin::Create(GURL("https://www.foo.com")),
             url::Origin::Create(GURL("https://www.bar.com"))),
         net::CookiePartitionKey::FromURLForTesting(
             GURL("https://subdomain.bar.com"))},
        {StorageKey::CreateWithNonce(
             url::Origin::Create(GURL("https://www.example.com")), nonce),
         net::CookiePartitionKey::FromURLForTesting(
             GURL("https://www.example.com"), nonce)},
    };
    for (const auto& test_case : test_cases) {
      EXPECT_EQ(test_case.expected,
                test_case.storage_key.ToCookiePartitionKey());
    }
  }
}

}  // namespace blink
