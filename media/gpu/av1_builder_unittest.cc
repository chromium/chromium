// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_builder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AV1BuilderTest : public ::testing::Test {
 public:
  AV1BuilderTest() = default;
  ~AV1BuilderTest() override = default;
};

TEST_F(AV1BuilderTest, AV1BitstreamBuilderOutstandingBits) {
  const size_t kExpectedDataBits = 8;
  AV1BitstreamBuilder packed_data;
  packed_data.Write(0, 3);
  packed_data.WriteBool(false);
  packed_data.PutAlignBits();
  EXPECT_EQ(packed_data.OutstandingBits(), kExpectedDataBits);
}

TEST_F(AV1BuilderTest, AV1BitstreamBuilderWriteOBUHeader) {
  const std::vector<uint8_t> expected_packed_data = {0b00010010, 0b00000011,
                                                     0b10100000};
  AV1BitstreamBuilder packed_obu_header;
  packed_obu_header.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                                   /*extension_flag=*/false,
                                   /*has_size=*/true);
  packed_obu_header.WriteValueInLeb128(3);
  packed_obu_header.WriteBool(true);
  packed_obu_header.Write(1, 2);
  EXPECT_EQ(std::move(packed_obu_header).Flush(), expected_packed_data);
}

TEST_F(AV1BuilderTest, AV1BitstreamBuilderAppendBitstreamBuffer) {
  const std::vector<uint8_t> expected_packed_data = {0b11010100, 0b11000000};
  AV1BitstreamBuilder append_data;
  append_data.Write(9, 5);
  append_data.PutTrailingBits();

  AV1BitstreamBuilder packed_data;
  packed_data.Write(6, 3);
  packed_data.WriteBool(true);
  packed_data.AppendBitstreamBuffer(std::move(append_data));
  packed_data.PutAlignBits();
  EXPECT_EQ(std::move(packed_data).Flush(), expected_packed_data);
}

}  // namespace media
