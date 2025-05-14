// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_event_params.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/cloneable_message.h"

namespace content {

namespace {

const char kSerializedDataLabel[] = "; Serialized Data: ";
const char kUrlsWithMetadataLabel[] = "; URLs With Metadata: ";

void SetCloneableMessageWithByteArray(blink::CloneableMessage& message,
                                      const base::span<const uint8_t>& data) {
  message.encoded_message = data;
  message.EnsureDataIsOwned();
}

std::string GetSerializedDataDirectFromBytes(
    const SharedStorageEventParams& params) {
  return params.serialized_data ? params.serialized_data.value()
                                : "std::nullopt";
}

std::string GetSerializedDataFromInsertionOperator(
    const SharedStorageEventParams& params) {
  std::ostringstream ss;
  ss << params;
  std::string params_str = ss.str();
  size_t serialized_data_label_pos = params_str.find(kSerializedDataLabel);
  CHECK_GT(serialized_data_label_pos, 0);
  size_t start =
      serialized_data_label_pos + std::string(kSerializedDataLabel).size();
  size_t end = params_str.find(kUrlsWithMetadataLabel, start);
  CHECK_GT(end, 0);

  return params_str.substr(start, end - start);
}

std::vector<uint8_t> GetBytes(std::string str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

TEST(SharedStorageEventParamsTest, NonASCIISerializedData_EscapedByInsertion) {
  uint8_t array[] = {0,  1,  7,  8,  9,  10, 11, 12,  13,  14, 33,
                     34, 38, 39, 47, 65, 81, 92, 127, 200, 255};
  auto data = base::span<const uint8_t>(array);
  blink::CloneableMessage serialized_data_message;
  SetCloneableMessageWithByteArray(serialized_data_message, data);

  auto params = SharedStorageEventParams::CreateForRunForTesting(
      "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
      SharedStorageEventParams::PrivateAggregationConfigWrapper(),
      serialized_data_message, /*worklet_ordinal_id=*/0,
      /*worklet_devtools_token=*/base::UnguessableToken::Create());

  EXPECT_FALSE(base::IsStringUTF8(GetSerializedDataDirectFromBytes(params)));

  std::string serialized_data_str_from_insertion_op =
      GetSerializedDataFromInsertionOperator(params);
  EXPECT_TRUE(base::IsStringASCII(serialized_data_str_from_insertion_op));

  // We cannot easily use `EXPECT_EQ` to compare
  // `serialized_data_str_from_insertion_op` against an expected string literal,
  // as the macro will escape `serialized_data_str_from_insertion_op` again.
  EXPECT_THAT(
      GetBytes(serialized_data_str_from_insertion_op),
      testing::ElementsAre(92, 48, 92, 120, 48, 49, 92, 97, 92, 98, 92, 116, 92,
                           110, 92, 118, 92, 102, 92, 114, 92, 120, 48, 69, 33,
                           92, 34, 38, 92, 39, 47, 65, 81, 92, 92, 92, 120, 55,
                           70, 92, 120, 67, 56, 92, 120, 70, 70));
}

TEST(SharedStorageEventParamsTest, ASCIISerializedData_UnchangedByInsertion) {
  auto bytes = GetBytes("hello world");
  auto data = base::span<const uint8_t>(bytes);
  blink::CloneableMessage serialized_data_message;
  SetCloneableMessageWithByteArray(serialized_data_message, data);

  auto params = SharedStorageEventParams::CreateForRunForTesting(
      "test-operation", /*operation_id=*/0, /*keep_alive=*/false,
      SharedStorageEventParams::PrivateAggregationConfigWrapper(),
      serialized_data_message, /*worklet_ordinal_id=*/0,
      /*worklet_devtools_token=*/base::UnguessableToken::Create());

  std::string serialized_data_direct_from_bytes =
      GetSerializedDataDirectFromBytes(params);
  EXPECT_TRUE(base::IsStringASCII(serialized_data_direct_from_bytes));
  EXPECT_EQ("hello world", serialized_data_direct_from_bytes);

  std::string serialized_data_str_from_insertion_op =
      GetSerializedDataFromInsertionOperator(params);
  EXPECT_TRUE(base::IsStringASCII(serialized_data_str_from_insertion_op));
  EXPECT_EQ("hello world", serialized_data_str_from_insertion_op);
}

}  // namespace content
