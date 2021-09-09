// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key_parsing_utils.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PublicKeyParsingUtilsTest, WellFormedSingleKey_ParsedCorrectly) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {PublicKey("abcd", kABCD1234AsBytes)}, keys));
}

TEST(PublicKeyParsingUtilsTest, WellFormedMultipleKeys_ParsedCorrectly) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234"
                },
                {
                    "id" : "efgh",
                    "key": "EFGH5678"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {PublicKey("abcd", kABCD1234AsBytes),
       PublicKey("efgh", kEFGH5678AsBytes)},
      keys));
}

TEST(PublicKeyParsingUtilsTest, MalformedMissingId_EmptyResult) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "key" : "ABCD1234"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedMissingKey_EmptyResult) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedKeyNotValidBase64_EmptyResult) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD-1234"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, WellFormedAndMalformedKeys_EmptyResult) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234"
                },
                {
                    "id" : "efgh"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

TEST(PublicKeyParsingUtilsTest, MalformedKeyDuplicateKeyId_EmptyResult) {
  std::string json_string = R"(
        {
            "version" : "",
            "keys" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234"
                },
                {
                    "id" : "abcd",
                    "key" : "EFGH5678"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(keys.empty());
}

}  // namespace content
