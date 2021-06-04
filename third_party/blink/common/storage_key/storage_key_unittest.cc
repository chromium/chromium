// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Test when a constructed StorageKey object should be considered valid/opaque.
TEST(StorageKeyTest, ConstructionValidity) {
  StorageKey empty = StorageKey();
  EXPECT_TRUE(empty.opaque());

  url::Origin valid_origin = url::Origin::Create(GURL("https://example.com"));
  StorageKey valid = StorageKey(valid_origin);
  EXPECT_FALSE(valid.opaque());

  url::Origin invalid_origin =
      url::Origin::Create(GURL("I'm not a valid URL."));
  StorageKey invalid = StorageKey(invalid_origin);
  EXPECT_TRUE(invalid.opaque());
}

// Test that StorageKeys are/aren't equivalent as expected.
TEST(StorageKeyTest, Equivalance) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  url::Origin origin3 = url::Origin();
  url::Origin origin4 =
      url::Origin();  // Creates a different opaque origin than origin3.

  StorageKey key1_origin1 = StorageKey(origin1);
  StorageKey key2_origin1 = StorageKey(origin1);
  StorageKey key3_origin2 = StorageKey(origin2);

  StorageKey key4_origin3 = StorageKey(origin3);
  StorageKey key5_origin3 = StorageKey(origin3);
  StorageKey key6_origin4 = StorageKey(origin4);
  EXPECT_TRUE(key4_origin3.opaque());
  EXPECT_TRUE(key5_origin3.opaque());
  EXPECT_TRUE(key6_origin4.opaque());

  // All are equivalent to themselves
  EXPECT_EQ(key1_origin1, key1_origin1);
  EXPECT_EQ(key2_origin1, key2_origin1);
  EXPECT_EQ(key3_origin2, key3_origin2);
  EXPECT_EQ(key4_origin3, key4_origin3);
  EXPECT_EQ(key5_origin3, key5_origin3);
  EXPECT_EQ(key6_origin4, key6_origin4);

  // StorageKeys created from the same origins are equivalent.
  EXPECT_EQ(key1_origin1, key2_origin1);
  EXPECT_EQ(key4_origin3, key5_origin3);

  // StorageKeys created from different origins are not equivalent.
  EXPECT_NE(key1_origin1, key3_origin2);
  EXPECT_NE(key4_origin3, key6_origin4);
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

  EXPECT_TRUE(key1.has_value());
  EXPECT_FALSE(key1->opaque());
  EXPECT_TRUE(key2.has_value());
  EXPECT_FALSE(key2->opaque());
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

  EXPECT_FALSE(key1.opaque());
  EXPECT_EQ(key1, StorageKey(url::Origin::Create(GURL(example))));
  EXPECT_TRUE(key2.opaque());
  EXPECT_TRUE(key3.opaque());
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

}  // namespace blink
