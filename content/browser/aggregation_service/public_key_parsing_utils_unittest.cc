// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key_parsing_utils.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PublicKeyParsingUtilsTest, WellFormedSingleKey_ParsedCorrectly) {
  std::string json_string = R"(
        {
            "1.0" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234",
                    "not_before": "1623000000000",
                    "not_after" : "1633000000000"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {PublicKey("abcd", kABCD1234AsBytes,
                 base::Time::FromJavaTime(1623000000000),
                 base::Time::FromJavaTime(1633000000000))},
      keys));
}

TEST(PublicKeyParsingUtilsTest, WellFormedMultipleKeys_ParsedCorrectly) {
  std::string json_string = R"(
        {
            "1.0" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234",
                    "not_before": "1623000000000",
                    "not_after" : "1633000000000"
                },
                {
                    "id" : "efgh",
                    "key": "EFGH5678",
                    "not_before": "1622500000000",
                    "not_after" : "1632500000000"
                }
            ]
        }
    )";

  absl::optional<base::Value> json_object = base::JSONReader::Read(json_string);
  ASSERT_TRUE(json_object) << "Incorrectly formatted JSON string.";

  std::vector<PublicKey> keys =
      aggregation_service::GetPublicKeys(json_object.value());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      {PublicKey("abcd", kABCD1234AsBytes,
                 base::Time::FromJavaTime(1623000000000),
                 base::Time::FromJavaTime(1633000000000)),
       PublicKey("efgh", kEFGH5678AsBytes,
                 base::Time::FromJavaTime(1622500000000),
                 base::Time::FromJavaTime(1632500000000))},
      keys));
}

TEST(PublicKeyParsingUtilsTest, MalformedMissingId_EmptyResult) {
  std::string json_string = R"(
        {
            "1.0" : [
                {
                    "key" : "ABCD1234",
                    "not_before": "1623000000000",
                    "not_after" : "1633000000000"
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
            "1.0" : [
                {
                    "id" : "abcd",
                    "not_before": "1623000000000",
                    "not_after" : "1633000000000"
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

TEST(PublicKeyParsingUtilsTest, MalformedMissingNotBefore_EmptyResult) {
  std::string json_string = R"(
        {
            "1.0" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234",
                    "not_after" : "1633000000000"
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

TEST(PublicKeyParsingUtilsTest, MalformedMissingNotAfter_EmptyResult) {
  std::string json_string = R"(
        {
            "1.0" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD1234",
                    "not_before": "1623000000000"
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
            "1.0" : [
                {
                    "id" : "abcd",
                    "key" : "ABCD-1234",
                    "not_before": "1623000000000",
                    "not_after" : "1633000000000"
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
