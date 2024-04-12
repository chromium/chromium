// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/box_byte_stream.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

enum class DataType {
  kPlaceHolder = 0,
  kType_8 = 1,
  kType_16 = 2,
  kType_32 = 3,
  kType_64 = 4,
  kType_Bytes = 5,
  kType_String = 6,
  kEndBox = 7,
};

struct DataOrder {
  DataType type;
  uint64_t value;
};

TEST(BoxByteStreamTest, Default) {
  // Test basic Write APIs of the BoxByteStream.
  BoxByteStream box_byte_stream;

  DataOrder test_data[] = {
      {DataType::kType_8, 0x48},
      {DataType::kPlaceHolder, /*total_size=*/44},
      {DataType::kType_16, 0x1617},
      {DataType::kType_32, 0x32333435},
      {DataType::kType_64, 0x64646667686970},
      {DataType::kType_Bytes, 0x12345678901234},
      {DataType::kPlaceHolder, /*total_size=*/9},
      {DataType::kType_8, 0x28},
      {DataType::kEndBox, 0x0},
      {DataType::kType_16, 0x0},
      {DataType::kType_32, 0x0},
      {DataType::kEndBox, 0x0},
      {DataType::kType_String, reinterpret_cast<uint64_t>("")},
      {DataType::kType_String, reinterpret_cast<uint64_t>("abcdabcd")}};
  for (auto& data : test_data) {
    switch (data.type) {
      case DataType::kPlaceHolder:
        box_byte_stream.StartBox(mp4::FOURCC_MOOV);
        break;
      case DataType::kEndBox:
        box_byte_stream.EndBox();
        break;
      case DataType::kType_8:
        box_byte_stream.WriteU8(static_cast<uint8_t>(data.value));
        break;
      case DataType::kType_16:
        box_byte_stream.WriteU16(static_cast<uint16_t>(data.value));
        break;
      case DataType::kType_32:
        box_byte_stream.WriteU32(static_cast<uint32_t>(data.value));
        break;
      case DataType::kType_64:
        box_byte_stream.WriteU64(data.value);
        break;
      case DataType::kType_Bytes:
        box_byte_stream.WriteBytes(&data.value, 7);
        break;
      case DataType::kType_String:
        box_byte_stream.WriteString(reinterpret_cast<char*>(data.value));
        break;
    }
  }

  std::vector<uint8_t> written_data = box_byte_stream.Flush();
  auto reader = base::SpanReader(base::span(written_data));
  for (auto& data : test_data) {
    uint64_t ret_value = 0;
    switch (data.type) {
      case DataType::kPlaceHolder: {
        uint32_t val;
        EXPECT_TRUE(reader.ReadU32BigEndian(val));
        base::byte_span_from_ref(ret_value).first<4>().copy_from(
            base::byte_span_from_ref(val));
        EXPECT_EQ(data.value, ret_value);
        EXPECT_TRUE(reader.ReadU32BigEndian(val));
        base::byte_span_from_ref(ret_value).first<4>().copy_from(
            base::byte_span_from_ref(val));
        EXPECT_EQ(mp4::FOURCC_MOOV, ret_value);
        break;
      }
      case DataType::kEndBox:
        break;
      case DataType::kType_8: {
        uint8_t val;
        EXPECT_TRUE(reader.ReadU8BigEndian(val));
        base::byte_span_from_ref(ret_value).first<1>().copy_from(
            base::byte_span_from_ref(val));
        EXPECT_EQ(data.value, ret_value);
        break;
      }
      case DataType::kType_16: {
        uint16_t val;
        EXPECT_TRUE(reader.ReadU16BigEndian(val));
        base::byte_span_from_ref(ret_value).first<2>().copy_from(
            base::byte_span_from_ref(val));
        EXPECT_EQ(data.value, ret_value);
        break;
      }
      case DataType::kType_32: {
        uint32_t val;
        EXPECT_TRUE(reader.ReadU32BigEndian(val));
        base::byte_span_from_ref(ret_value).first<4>().copy_from(
            base::byte_span_from_ref(val));
        EXPECT_EQ(data.value, ret_value);
        break;
      }
      case DataType::kType_64:
        EXPECT_TRUE(reader.ReadU64BigEndian(ret_value));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_Bytes:
        EXPECT_TRUE(
            reader.ReadCopy(base::byte_span_from_ref(ret_value).first<7>()));
        EXPECT_EQ(data.value, ret_value);
        break;
      case DataType::kType_String:
        std::string expected_string = reinterpret_cast<char*>(data.value);
        if (expected_string.empty()) {
          EXPECT_TRUE(
              reader.ReadCopy(base::byte_span_from_ref(ret_value).first<1>()));
          EXPECT_EQ(ret_value, 0u);
        } else {
          base::span<uint8_t> val;
          EXPECT_TRUE(reader.ReadInto(expected_string.size(), val));
          EXPECT_EQ(expected_string, base::as_string_view(val));
        }

        break;
    }
  }
}

TEST(BoxByteStreamTest, GrowLimit) {
  // Test GrowWriter feature.
  BoxByteStream box_byte_stream;

  box_byte_stream.StartBox(mp4::FOURCC_MOOV);
  for (int i = 0; i < BoxByteStream::kDefaultBufferLimit; ++i) {
    box_byte_stream.WriteU8(0);
  }
  box_byte_stream.StartBox(mp4::FOURCC_TRAK);
  box_byte_stream.WriteU16(0x1617);
  box_byte_stream.WriteU32(0);
  box_byte_stream.EndBox();
  box_byte_stream.EndBox();

  std::vector<uint8_t> written_data = box_byte_stream.Flush();
  auto reader = base::SpanReader(base::span(written_data));

  uint32_t expected_total_size =
      8u + uint32_t{BoxByteStream::kDefaultBufferLimit} + 8u + 2u + 4u;
  uint32_t value;
  reader.ReadU32BigEndian(value);
  EXPECT_EQ(expected_total_size, value);
  reader.ReadU32BigEndian(value);
  EXPECT_EQ(mp4::FOURCC_MOOV, value);

  reader.Skip(size_t{BoxByteStream::kDefaultBufferLimit});
  reader.ReadU32BigEndian(value);
  EXPECT_EQ(14u, value);
  reader.ReadU32BigEndian(value);
  EXPECT_EQ(mp4::FOURCC_TRAK, value);

  uint16_t value16;
  reader.ReadU16BigEndian(value16);
  EXPECT_EQ(0x1617u, value16);
  reader.ReadU32BigEndian(value);
  EXPECT_EQ(0u, value);
}

TEST(BoxByteStreamTest, EndBoxAndFlushDiff) {
  // Test Flush and EndBox difference.
  // EndBox use.
  {
    BoxByteStream box_byte_stream;

    // <parent>
    box_byte_stream.StartBox(mp4::FOURCC_MOOV);
    box_byte_stream.WriteU64(0);
    {
      // <child 1>
      box_byte_stream.StartBox(mp4::FOURCC_TRAK);
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);

      box_byte_stream.WriteU32(0x1617);
      {
        // <grand child 1>
        box_byte_stream.StartBox(mp4::FOURCC_MDIA);
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 3u);
        box_byte_stream.WriteU16(0);
        box_byte_stream.EndBox();
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);
      }
      box_byte_stream.EndBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 1u);

      // <child 2>
      box_byte_stream.StartBox(mp4::FOURCC_MVEX);
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);
      box_byte_stream.WriteU32(0);
      box_byte_stream.EndBox();
      EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 1u);

      {
        // <grand child 2 and the last box>
        box_byte_stream.StartBox(mp4::FOURCC_MFRO);
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 2u);
        // It will have top box's total size.
        box_byte_stream.WriteU32(box_byte_stream.size() + 4);
        box_byte_stream.EndBox();
        EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 1u);
      }
    }
    box_byte_stream.EndBox();
    EXPECT_EQ(box_byte_stream.GetSizeOffsetsForTesting().size(), 0u);

    // Read.
    std::vector<uint8_t> written_data = box_byte_stream.Flush();
    auto reader = base::SpanReader(base::span(written_data));

    uint32_t parent;
    uint32_t fourcc;
    constexpr uint32_t kMoovTopBoxSize = 62u;
    reader.ReadU32BigEndian(parent);
    EXPECT_EQ(kMoovTopBoxSize, parent);
    reader.ReadU32BigEndian(fourcc);
    EXPECT_EQ(mp4::FOURCC_MOOV, fourcc);
    reader.Skip(8u);

    uint32_t child_1;
    reader.ReadU32BigEndian(child_1);
    EXPECT_EQ(22u, child_1);
    reader.ReadU32BigEndian(fourcc);
    EXPECT_EQ(mp4::FOURCC_TRAK, fourcc);
    reader.Skip(4u);

    uint32_t grand_child_1;
    reader.ReadU32BigEndian(grand_child_1);
    EXPECT_EQ(10u, grand_child_1);
    reader.ReadU32BigEndian(fourcc);
    EXPECT_EQ(mp4::FOURCC_MDIA, fourcc);
    reader.Skip(2u);

    uint32_t child_2;
    reader.ReadU32BigEndian(child_2);
    EXPECT_EQ(12u, child_2);
    reader.ReadU32BigEndian(fourcc);
    EXPECT_EQ(mp4::FOURCC_MVEX, fourcc);
    reader.Skip(4u);

    uint32_t grand_child_2;
    reader.ReadU32BigEndian(grand_child_2);
    EXPECT_EQ(12u, grand_child_2);
    reader.ReadU32BigEndian(fourcc);
    EXPECT_EQ(mp4::FOURCC_MFRO, fourcc);

    uint32_t field_top_box_size;
    reader.ReadU32BigEndian(field_top_box_size);
    EXPECT_EQ(kMoovTopBoxSize, field_top_box_size);
  }
}

TEST(BoxByteStreamTest, OffsetPlaceholderAndFlush) {
  // Test Flush and EndBox difference.
  // EndBox use.
  BoxByteStream box_byte_stream;
  {
    // `moof`
    box_byte_stream.StartBox(mp4::FOURCC_MOOF);

    // `traf`
    box_byte_stream.StartBox(mp4::FOURCC_TRAF);

    // 'trun'
    box_byte_stream.StartBox(mp4::FOURCC_TRUN);
    box_byte_stream.WriteOffsetPlaceholder();
    box_byte_stream.WriteU32(0);
    box_byte_stream.WriteOffsetPlaceholder();
    box_byte_stream.EndBox();  // for FOURCC_TRUN.
    box_byte_stream.EndBox();  // for FOURCC_TRAF.
    box_byte_stream.EndBox();  // for FOURCC_MOOF.
  }

  std::vector<uint8_t> data(4000, 0);
  {
    // `moof`
    box_byte_stream.StartBox(mp4::FOURCC_MDAT);
    box_byte_stream.FlushCurrentOffset();

    box_byte_stream.WriteBytes(data.data(), data.size());
    box_byte_stream.EndBox();  // for FOURCC_MDAT.
    box_byte_stream.FlushCurrentOffset();
  }

  std::vector<uint8_t> written_data = box_byte_stream.Flush();
  auto reader = base::SpanReader(base::span(written_data));
  uint32_t fourcc;
  reader.Skip(4u);
  reader.ReadU32BigEndian(fourcc);
  EXPECT_EQ(mp4::FOURCC_MOOF, fourcc);
  reader.Skip(4u);
  reader.ReadU32BigEndian(fourcc);
  EXPECT_EQ(mp4::FOURCC_TRAF, fourcc);
  reader.Skip(4u);
  reader.ReadU32BigEndian(fourcc);
  EXPECT_EQ(mp4::FOURCC_TRUN, fourcc);

  // placeholder.
  uint32_t data_offset;
  reader.ReadU32BigEndian(data_offset);
  EXPECT_EQ(44u, data_offset);
  reader.Skip(4u);
  reader.ReadU32BigEndian(data_offset);
  EXPECT_EQ(44u + data.size(), data_offset);
}

}  // namespace media
