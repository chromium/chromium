// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key_parsing_utils.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PublicKeyParsingUtilsTest, WellFormedSingleKey_ParsedCorrectly) {
  aggregation_service::TestHpkeKey generated_key{/*key_id=*/"abcd"};

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                   "id": "abcd",
                   "key": "$1"
                }
            ]
         })",
      {generated_key.GetPublicKeyBase64()}, /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {generated_key.GetPublicKey()}, keys));
}

TEST(PublicKeyParsingUtilsTest, WellFormedMultipleKeys_ParsedCorrectly) {
  aggregation_service::TestHpkeKey generated_key_1{/*key_id=*/"abcd"};
  aggregation_service::TestHpkeKey generated_key_2{/*key_id=*/"efgh"};

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                    "id": "abcd",
                    "key": "$1"
                },
                {
                    "id": "efgh",
                    "key": "$2"
                }
            ]
         })",
      {generated_key_1.GetPublicKeyBase64(),
       generated_key_2.GetPublicKeyBase64()},
      /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {generated_key_1.GetPublicKey(), generated_key_2.GetPublicKey()}, keys));
}

TEST(PublicKeyParsingUtilsTest, MalformedMissingId_EmptyResult) {
  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                    "key": "$1"
                }
            ]
         })",
      {aggregation_service::TestHpkeKey().GetPublicKeyBase64()},
      /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedMissingKey_EmptyResult) {
  std::string json_string = R"(
        {
            "keys": [
                {
                    "id": "abcd"
                }
            ]
        }
    )";

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedKeyNotValidBase64_EmptyResult) {
  std::string invalid_base64 =
      aggregation_service::TestHpkeKey().GetPublicKeyBase64();
  // Replace a character with an invalid one.
  invalid_base64[5] = '-';

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                   "id": "abcd",
                   "key": "$1"
                }
            ]
         })",
      {invalid_base64}, /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedKeyWrongLength_EmptyResult) {
  std::string wrong_length_key =
      aggregation_service::TestHpkeKey().GetPublicKeyBase64();
  wrong_length_key.resize(16);

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                   "id": "abcd",
                   "key": "$1"
                }
            ]
         })",
      {wrong_length_key}, /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, WellFormedAndMalformedKeys_EmptyResult) {
  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                    "id": "abcd",
                    "key": "$1"
                },
                {
                    "id": "efgh"
                }
            ]
         }
    )",
      {aggregation_service::TestHpkeKey().GetPublicKeyBase64()},
      /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedKeyDuplicateKeyId_EmptyResult) {
  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "keys": [
                {
                    "id": "abcd",
                    "key": "$1"
                },
                {
                    "id": "abcd",
                    "key": "$2"
                }
            ]
         })",
      {aggregation_service::TestHpkeKey(/*key_id=*/"abcd").GetPublicKeyBase64(),
       aggregation_service::TestHpkeKey(/*key_id=*/"abcd")
           .GetPublicKeyBase64()},
      /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, VersionFieldSpecified_FieldIgnored) {
  aggregation_service::TestHpkeKey generated_key{/*key_id=*/"abcd"};

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "version": "",
            "keys": [
                {
                   "id": "abcd",
                   "key": "$1"
                }
            ]
         })",
      {generated_key.GetPublicKeyBase64()}, /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {generated_key.GetPublicKey()}, keys));
}

TEST(PublicKeyParsingUtilsTest, ExtraUnexpectedField_FieldIgnored) {
  aggregation_service::TestHpkeKey generated_key{/*key_id=*/"abcd"};

  std::string json_string = base::ReplaceStringPlaceholders(
      R"({
            "unexpected_field": [{ "foo": 123, "bar": "baz" }, {}],
            "keys": [
                {
                   "id": "abcd",
                   "key": "$1"
                }
            ]
         })",
      {generated_key.GetPublicKeyBase64()}, /*offsets=*/nullptr);

  std::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {generated_key.GetPublicKey()}, keys));
}

}  // namespace content
