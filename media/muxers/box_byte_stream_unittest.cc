// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/big_endian.h"
#include "base/test/task_environment.h"
#include "media/muxers/box_byte_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

enum class DataType {
  kPlaceHolder = 0,
  kType_8 = 1,
  kType_16 = 2,
  kType_24 = 3,
  kType_32 = 4,
  kType_64 = 5,
};

struct DataOrder {
  DataType type;
  uint64_t value;
};

TEST(BoxByteStreamTest, Default) {
  // Test basic Write APIs of the BoxByteStream.
  BoxByteStream box_byte_stream;

  DataOrder test_data[] = {
      {DataType::kType_8, 0x48},        {DataType::kPlaceHolder, 32},
      {DataType::kType_16, 0x1617},     {DataType::kType_24, 0x242526},
      {DataType::kType_32, 0x32333435}, {DataType::kType_64, 0x64646667686970},
      {DataType::kPlaceHolder, 11},     {DataType::kType_8, 0x28},
      {DataType::kType_16, 0x0},        {DataType::kType_32, 0x0}};
  for (auto& data : test_data) {
    switch (data.type) {
      case DataType::kPlaceHolder:
        box_byte_stream.StartBox();
        break;
      case DataType::kType_8:
        box_byte_stream.WriteU8(static_cast<uint8_t>(data.value));
        break;
      case DataType::kType_16:
        box_byte_stream.WriteU16(static_cast<uint16_t>(data.value));
        break;
      case DataType::kType_24:
        box_byte_stream.WriteU24(static_cast<uint32_t>(data.value));
        break;
      case DataType::kType_32:
        box_byte_stream.WriteU32(static_cast<uint32_t>(data.value));
        break;
      case DataType::kType_64:
        box_byte_stream.WriteU64(data.value);
        break;
    }
  }

  std::vector<uint8_t> written_data = box_byte_stream.Flush();
  base::BigEndianReader reader(written_data.data(), written_data.size());
  for (auto& data : test_data) {
    uint64_t ret_value = 0;
    switch (data.type) {
      case DataType::kPlaceHolder:
        EXPECT_TRUE(reader.ReadU32(reinterpret_cast<uint32_t*>(&ret_value)));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_8:
        EXPECT_TRUE(reader.ReadU8(reinterpret_cast<uint8_t*>(&ret_value)));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_16:
        EXPECT_TRUE(reader.ReadU16(reinterpret_cast<uint16_t*>(&ret_value)));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_24:
        EXPECT_TRUE(reader.ReadBytes(&ret_value, 3));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_32:
        EXPECT_TRUE(reader.ReadU32(reinterpret_cast<uint32_t*>(&ret_value)));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_64:
        EXPECT_TRUE(reader.ReadU64(&ret_value));
        EXPECT_EQ(data.value, ret_value);
        break;
    }
  }
}

TEST(BoxByteStreamTest, GrowLimit) {
  // Test GrowWriter feature.
  BoxByteStream box_byte_stream;

  box_byte_stream.StartBox();
  for (int i = 0; i < BoxByteStream::kDefaultBufferLimit; ++i) {
    box_byte_stream.WriteU8(0);
  }
  box_byte_stream.StartBox();
  box_byte_stream.WriteU16(0x1617);
  box_byte_stream.WriteU32(0);
  box_byte_stream.EndBox();
  box_byte_stream.EndBox();

  std::vector<uint8_t> written_data = box_byte_stream.Flush();
  base::BigEndianReader reader(written_data.data(), written_data.size());

  uint32_t expected_total_size =
      4 + BoxByteStream::kDefaultBufferLimit + 4 + 2 + 4;
  uint32_t value;
  reader.ReadU32(&value);
  EXPECT_EQ(expected_total_size, value);

  reader.Skip(BoxByteStream::kDefaultBufferLimit);
  reader.ReadU32(&value);
  EXPECT_EQ(10u, value);

  uint16_t value16;
  reader.ReadU16(&value16);
  EXPECT_EQ(0x1617u, value16);
  reader.ReadU32(&value);
  EXPECT_EQ(0u, value);
}

TEST(BoxByteStreamTest, EndBoxAndFlushDiff) {
  // Test Flush and EndBox difference.
  // EndBox use.
  {
    BoxByteStream box_byte_stream;

    // <parent>
    box_byte_stream.StartBox();
    box_byte_stream.WriteU64(0);
    {
      // <child 1>
      box_byte_stream.StartBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);

      box_byte_stream.WriteU32(0x1617);
      {
        // <grand child 1>
        box_byte_stream.StartBox();
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 3u);
        box_byte_stream.WriteU16(0);
        box_byte_stream.EndBox();
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);
      }
      box_byte_stream.EndBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 1u);

      // <child 2>
      box_byte_stream.StartBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);
      box_byte_stream.WriteU32(0);
      box_byte_stream.EndBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 1u);
    }
    box_byte_stream.EndBox();
    EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 0u);

    // Read.
    std::vector<uint8_t> written_data = box_byte_stream.Flush();
    base::BigEndianReader reader(written_data.data(), written_data.size());

    uint32_t parent;
    reader.ReadU32(&parent);
    EXPECT_EQ(34u, parent);
    reader.Skip(8);

    uint32_t child_1;
    reader.ReadU32(&child_1);
    EXPECT_EQ(14u, child_1);
    reader.Skip(4);

    uint32_t grand_child_1;
    reader.ReadU32(&grand_child_1);
    EXPECT_EQ(6u, grand_child_1);
    reader.Skip(2);

    uint32_t child_2;
    reader.ReadU32(&child_2);
    EXPECT_EQ(8u, child_2);
  }

  // Flush use.
  {
    BoxByteStream box_byte_stream;

    // <parent>
    box_byte_stream.StartBox();
    box_byte_stream.WriteU64(0);
    {
      // <child 1>
      box_byte_stream.StartBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);

      box_byte_stream.WriteU32(0x1617);
      {
        // <grand child 1>
        box_byte_stream.StartBox();
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 3u);
        box_byte_stream.WriteU16(0);
      }

      // <child 2>
      box_byte_stream.StartBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 4u);
      box_byte_stream.WriteU32(0);
    }

    // Read.
    EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 4u);
    std::vector<uint8_t> written_data = box_byte_stream.Flush();
    base::BigEndianReader reader(written_data.data(), written_data.size());

    uint32_t parent;
    reader.ReadU32(&parent);
    EXPECT_EQ(34u, parent);
    reader.Skip(8);

    uint32_t child_1;
    reader.ReadU32(&child_1);
    EXPECT_EQ(22u, child_1);
    reader.Skip(4);

    uint32_t grand_child_1;
    reader.ReadU32(&grand_child_1);
    EXPECT_EQ(14u, grand_child_1);
    reader.Skip(2);

    uint32_t child_2;
    reader.ReadU32(&child_2);
    EXPECT_EQ(8u, child_2);
  }
}

}  // namespace media
