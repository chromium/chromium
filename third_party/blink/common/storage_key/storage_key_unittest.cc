// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Opaqueness here is used as a way of checking for "correctly constructed" in
// most tests.
//
// Why not call it IsValid()? Because some tests actually want to check for
// opaque origins.
bool IsOpaque(const blink::StorageKey& key) {
  return key.origin().opaque();
}

}  // namespace

namespace blink {

// Test when a constructed StorageKey object should be considered valid/opaque.
TEST(StorageKeyTest, ConstructionValidity) {
  StorageKey empty = StorageKey();
  EXPECT_TRUE(IsOpaque(empty));

  url::Origin valid_origin = url::Origin::Create(GURL("https://example.com"));
  StorageKey valid = StorageKey(valid_origin);
  EXPECT_FALSE(IsOpaque(valid));

  url::Origin invalid_origin =
      url::Origin::Create(GURL("I'm not a valid URL."));
  StorageKey invalid = StorageKey(invalid_origin);
  EXPECT_TRUE(IsOpaque(invalid));
}

// Test that StorageKeys are/aren't equivalent as expected.
TEST(StorageKeyTest, Equivalence) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  url::Origin origin3 = url::Origin();
  url::Origin origin4 =
      url::Origin();  // Creates a different opaque origin than origin3.

  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();

  StorageKey key1_origin1 = StorageKey(origin1);
  StorageKey key2_origin1 = StorageKey(origin1);
  StorageKey key3_origin2 = StorageKey(origin2);

  StorageKey key4_origin3 = StorageKey(origin3);
  StorageKey key5_origin3 = StorageKey(origin3);
  StorageKey key6_origin4 = StorageKey(origin4);
  EXPECT_TRUE(IsOpaque(key4_origin3));
  EXPECT_TRUE(IsOpaque(key5_origin3));
  EXPECT_TRUE(IsOpaque(key6_origin4));

  StorageKey key7_origin1_nonce1 = StorageKey::CreateWithNonce(origin1, nonce1);
  StorageKey key8_origin1_nonce1 = StorageKey::CreateWithNonce(origin1, nonce1);
  StorageKey key9_origin1_nonce2 = StorageKey::CreateWithNonce(origin1, nonce2);
  StorageKey key10_origin2_nonce1 =
      StorageKey::CreateWithNonce(origin2, nonce1);

  // All are equivalent to themselves
  EXPECT_EQ(key1_origin1, key1_origin1);
  EXPECT_EQ(key2_origin1, key2_origin1);
  EXPECT_EQ(key3_origin2, key3_origin2);
  EXPECT_EQ(key4_origin3, key4_origin3);
  EXPECT_EQ(key5_origin3, key5_origin3);
  EXPECT_EQ(key6_origin4, key6_origin4);
  EXPECT_EQ(key7_origin1_nonce1, key7_origin1_nonce1);
  EXPECT_EQ(key8_origin1_nonce1, key8_origin1_nonce1);
  EXPECT_EQ(key9_origin1_nonce2, key9_origin1_nonce2);
  EXPECT_EQ(key10_origin2_nonce1, key10_origin2_nonce1);

  // StorageKeys created from the same origins are equivalent.
  EXPECT_EQ(key1_origin1, key2_origin1);
  EXPECT_EQ(key4_origin3, key5_origin3);

  // StorageKeys created from different origins are not equivalent.
  EXPECT_NE(key1_origin1, key3_origin2);
  EXPECT_NE(key4_origin3, key6_origin4);
  EXPECT_NE(key7_origin1_nonce1, key10_origin2_nonce1);

  // StorageKeys created from the same origins and nonces are equivalent.
  EXPECT_EQ(key7_origin1_nonce1, key8_origin1_nonce1);

  // StorageKeys created from different nonces are not equivalent.
  EXPECT_NE(key7_origin1_nonce1, key9_origin1_nonce2);

  // StorageKeys created from different origin and different nonces are not
  // equivalent.
  EXPECT_NE(key9_origin1_nonce2, key10_origin2_nonce1);
}

// Test that StorageKeys Serialize to the expected value.
TEST(StorageKeyTest, Serialize) {
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

TEST(StorageKeyTest, SerializeForLocalStorage) {
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

// Test that deserialized StorageKeys are valid/opaque as expected.
TEST(StorageKeyTest, Deserialize) {
  std::string example = "https://example.com/";
  std::string test = "https://test.example/";
  std::string wrong = "I'm not a valid URL.";

  absl::optional<StorageKey> key1 = StorageKey::Deserialize(example);
  absl::optional<StorageKey> key2 = StorageKey::Deserialize(test);
  absl::optional<StorageKey> key3 = StorageKey::Deserialize(wrong);
  absl::optional<StorageKey> key4 = StorageKey::Deserialize(std::string());

  ASSERT_TRUE(key1.has_value());
  EXPECT_FALSE(IsOpaque(*key1));
  ASSERT_TRUE(key2.has_value());
  EXPECT_FALSE(IsOpaque(*key2));
  EXPECT_FALSE(key3.has_value());
  EXPECT_FALSE(key4.has_value());
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

TEST(StorageKeyTest, IsThirdPartyStoragePartitioningEnabled) {
  EXPECT_FALSE(StorageKey::IsThirdPartyStoragePartitioningEnabled());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);
  EXPECT_TRUE(StorageKey::IsThirdPartyStoragePartitioningEnabled());
}

}  // namespace blink
