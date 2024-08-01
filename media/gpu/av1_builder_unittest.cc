// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/av1_builder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AV1BuilderTest : public ::testing::Test {
 public:
  AV1BuilderTest() = default;
  ~AV1BuilderTest() override = default;

  AV1BitstreamBuilder::SequenceHeader MakeSequenceHeader() {
    AV1BitstreamBuilder::SequenceHeader seq_hdr;
    seq_hdr.profile = 1;
    seq_hdr.level = 12;
    seq_hdr.tier = 0;
    seq_hdr.frame_width_bits_minus_1 = 15;
    seq_hdr.frame_height_bits_minus_1 = 15;
    seq_hdr.width = 1280;
    seq_hdr.height = 720;
    seq_hdr.use_128x128_superblock = true;
    seq_hdr.enable_filter_intra = true;
    seq_hdr.enable_intra_edge_filter = true;
    seq_hdr.enable_interintra_compound = true;
    seq_hdr.enable_masked_compound = true;
    seq_hdr.enable_warped_motion = true;
    seq_hdr.enable_dual_filter = true;
    seq_hdr.enable_order_hint = true;
    seq_hdr.enable_jnt_comp = true;
    seq_hdr.enable_ref_frame_mvs = true;
    seq_hdr.order_hint_bits_minus_1 = 7;
    seq_hdr.enable_superres = true;
    seq_hdr.enable_cdef = true;
    seq_hdr.enable_restoration = true;
    return seq_hdr;
  }

  AV1BitstreamBuilder::FrameHeader MakeFrameHeader(uint32_t frame_id) {
    AV1BitstreamBuilder::FrameHeader pic_hdr;
    pic_hdr.frame_type = frame_id == 0 ? libgav1::FrameType::kFrameKey
                                       : libgav1::FrameType::kFrameInter;
    pic_hdr.error_resilient_mode = false;
    pic_hdr.disable_cdf_update = false;
    pic_hdr.disable_frame_end_update_cdf = false;
    pic_hdr.base_qindex = 100;
    pic_hdr.order_hint = frame_id;
    pic_hdr.filter_level[0] = 1;
    pic_hdr.filter_level[1] = 1;
    pic_hdr.filter_level_u = 1;
    pic_hdr.filter_level_v = 1;
    pic_hdr.sharpness_level = 1;
    pic_hdr.loop_filter_delta_enabled = false;
    pic_hdr.primary_ref_frame = 0;
    for (uint8_t& ref_idx : pic_hdr.ref_frame_idx) {
      ref_idx = 0;
    }
    pic_hdr.refresh_frame_flags = 1;
    for (uint32_t& ref_order_hint : pic_hdr.ref_order_hint) {
      ref_order_hint = 0;
    }
    for (int i = 0; i < 8; i++) {
      pic_hdr.cdef_y_pri_strength[i] = 0;
      pic_hdr.cdef_y_sec_strength[i] = 0;
      pic_hdr.cdef_uv_pri_strength[i] = 0;
      pic_hdr.cdef_uv_sec_strength[i] = 0;
    }
    pic_hdr.reduced_tx_set = true;
    pic_hdr.segmentation_enabled = false;

    return pic_hdr;
  }
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

TEST_F(AV1BuilderTest, BuildSequenceHeaderOBU) {
  const std::vector<uint8_t> expected_packed_data = {
      0b00100000, 0b00000000, 0b00000000, 0b01100011, 0b11111100,
      0b00010011, 0b11111100, 0b00001011, 0b00111101, 0b11111111,
      0b11001111, 0b11000000, 0b10100000};
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(MakeSequenceHeader());

  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  EXPECT_EQ(std::move(seq_header_obu).Flush(), expected_packed_data);
}

TEST_F(AV1BuilderTest, BuildFrameOBU) {
  const std::vector<uint8_t> expected_packed_keyframe = {
      0b00010000, 0b00000000, 0b01000110, 0b01000000, 0b00000000, 0b10000010,
      0b00001000, 0b00100101, 0b01100000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00011000};
  const std::vector<uint8_t> expected_packed_interframe = {
      0b00110000, 0b0000001,  0b00000000, 0b00100000, 0b00000000, 0b00000000,
      0b00000000, 0b10001100, 0b10000000, 0b00000001, 0b00000100, 0b00010000,
      0b01001010, 0b11000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00101000, 0b00000000};
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeSequenceHeader();
  AV1BitstreamBuilder frame_obu_key =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, MakeFrameHeader(0));
  AV1BitstreamBuilder frame_obu_inter =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, MakeFrameHeader(1));

  EXPECT_EQ(frame_obu_key.OutstandingBits() % 8, 0ull);
  EXPECT_EQ(frame_obu_inter.OutstandingBits() % 8, 0ull);
  EXPECT_EQ(std::move(frame_obu_key).Flush(), expected_packed_keyframe);
  EXPECT_EQ(std::move(frame_obu_inter).Flush(), expected_packed_interframe);
}

}  // namespace media
