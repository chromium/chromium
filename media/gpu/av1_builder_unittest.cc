// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/av1_builder.h"

#include <new>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {

using ::testing::ElementsAreArray;

class AV1BuilderTest : public ::testing::Test {
 public:
  AV1BuilderTest() {
    buffer_pool_ = std::make_unique<libgav1::BufferPool>(
        /*on_frame_buffer_size_changed=*/nullptr,
        /*get_frame_buffer=*/nullptr,
        /*release_frame_buffer=*/nullptr,
        /*callback_private_data=*/nullptr);
    av1_decoder_state_ = std::make_unique<libgav1::DecoderState>();
  }

  ~AV1BuilderTest() override = default;

  AV1BitstreamBuilder::SequenceHeader MakeSequenceHeader() {
    AV1BitstreamBuilder::SequenceHeader seq_hdr{};
    seq_hdr.profile = 0;
    seq_hdr.operating_points_cnt_minus_1 = 0;
    seq_hdr.level.at(0) = 12;
    seq_hdr.tier.at(0) = 0;
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

  // A version that mimics the default settings used by AV1 delegate.
  AV1BitstreamBuilder::SequenceHeader MakeDefaultSequenceHeader() {
    AV1BitstreamBuilder::SequenceHeader seq_hdr{};

    seq_hdr.profile = 0;
    seq_hdr.level[0] = 12;
    seq_hdr.tier[0] = 0;
    seq_hdr.operating_points_cnt_minus_1 = 0;
    seq_hdr.frame_width_bits_minus_1 = 15;
    seq_hdr.frame_height_bits_minus_1 = 15;
    seq_hdr.width = 1280;
    seq_hdr.height = 720;
    seq_hdr.order_hint_bits_minus_1 = 7;

    seq_hdr.use_128x128_superblock = false;
    seq_hdr.enable_filter_intra = false;
    seq_hdr.enable_intra_edge_filter = false;
    seq_hdr.enable_interintra_compound = false;
    seq_hdr.enable_masked_compound = false;
    seq_hdr.enable_warped_motion = false;
    seq_hdr.enable_dual_filter = false;
    seq_hdr.enable_order_hint = true;
    seq_hdr.enable_jnt_comp = false;
    seq_hdr.enable_ref_frame_mvs = false;
    seq_hdr.enable_superres = false;
    seq_hdr.enable_cdef = true;
    seq_hdr.enable_restoration = false;

    return seq_hdr;
  }

  AV1BitstreamBuilder::FrameHeader MakeFrameHeader(uint32_t frame_id) {
    AV1BitstreamBuilder::FrameHeader pic_hdr{};
    pic_hdr.frame_type = frame_id == 0 ? libgav1::FrameType::kFrameKey
                                       : libgav1::FrameType::kFrameInter;
    pic_hdr.error_resilient_mode = false;
    pic_hdr.disable_cdf_update = false;
    pic_hdr.disable_frame_end_update_cdf = false;
    pic_hdr.base_qindex = 100;
    pic_hdr.order_hint = frame_id;
    pic_hdr.filter_level.at(0) = 1;
    pic_hdr.filter_level.at(1) = 1;
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
    pic_hdr.cdef_bits = 3;
    pic_hdr.cdef_damping_minus_3 = 2;
    for (int i = 0; i < 8; i++) {
      pic_hdr.cdef_y_pri_strength.at(i) = 0;
      pic_hdr.cdef_y_sec_strength.at(i) = 0;
      pic_hdr.cdef_uv_pri_strength.at(i) = 0;
      pic_hdr.cdef_uv_sec_strength.at(i) = 0;
    }
    pic_hdr.reduced_tx_set = true;
    pic_hdr.tx_mode = libgav1::TxMode::kTxModeSelect;
    pic_hdr.segmentation_enabled = false;
    pic_hdr.allow_screen_content_tools = false;
    pic_hdr.allow_intrabc = false;

    return pic_hdr;
  }

  std::unique_ptr<libgav1::BufferPool> buffer_pool_;
  std::unique_ptr<libgav1::DecoderState> av1_decoder_state_;
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
                                   /*has_size=*/true);
  packed_obu_header.WriteValueInLeb128(3);
  packed_obu_header.WriteBool(true);
  packed_obu_header.Write(1, 2);
  EXPECT_EQ(std::move(packed_obu_header).Flush(), expected_packed_data);
}

TEST_F(AV1BuilderTest, AV1BitstreamBuilderWriteTemporalOBUHeader) {
  const std::vector<uint8_t> expected_packed_data = {0b00010110, 0b01000000,
                                                     0b00000011, 0b10100000};
  AV1BitstreamBuilder packed_obu_header;
  packed_obu_header.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                                   /*has_size=*/true,
                                   /*extension_flag=*/true,
                                   /*temporal_id=*/2);
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
      0b00000000, 0b00000000, 0b00000000, 0b01100011, 0b11111100,
      0b00010011, 0b11111100, 0b00001011, 0b00111101, 0b11111111,
      0b11001111, 0b11000000, 0b10100000};
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(MakeSequenceHeader());

  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  EXPECT_EQ(std::move(seq_header_obu).Flush(), expected_packed_data);
}

TEST_F(AV1BuilderTest, BuildTemporalSequenceHeaderOBU) {
  const std::vector<uint8_t> expected_packed_data = {
      0b00000000, 0b00100001, 0b00000111, 0b01100000, 0b01000000, 0b11011000,
      0b00010000, 0b00010110, 0b00111111, 0b11000001, 0b00111111, 0b11000000,
      0b10110011, 0b11011111, 0b11111100, 0b11111100, 0b00001010};
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeSequenceHeader();
  seq_hdr.operating_points_cnt_minus_1 = 2;  // Set scalability mode to L1T3.
  for (uint32_t i = 0; i <= seq_hdr.operating_points_cnt_minus_1; i++) {
    seq_hdr.level.at(i) = 12;
    seq_hdr.tier.at(i) = 0;
  }
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  EXPECT_EQ(std::move(seq_header_obu).Flush(), expected_packed_data);
}

TEST_F(AV1BuilderTest, BuildFrameOBU) {
  const std::vector<uint8_t> expected_packed_keyframe = {
      0b00010000, 0b00000000, 0b01000110, 0b01000000, 0b00000000, 0b10000010,
      0b00001000, 0b00100101, 0b01100000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b01100000};
  const std::vector<uint8_t> expected_packed_interframe = {
      0b00110000, 0b00000001, 0b00000000, 0b00100000, 0b00000000, 0b00000000,
      0b00000000, 0b01000110, 0b01000000, 0b00000000, 0b10000010, 0b00001000,
      0b00100101, 0b01100000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b01010000, 0b00000000};
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

TEST_F(AV1BuilderTest, BuildFrameOBUWithSegmentation) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  pic_hdr.primary_ref_frame = 7;
  pic_hdr.segmentation_enabled = true;
  pic_hdr.segmentation_update_map = true;
  pic_hdr.segmentation_temporal_update = false;
  pic_hdr.segmentation_update_data = true;
  // Enable SEG_LVEL_ALT_Q, SEG_LVL_LF_Y_V for segment 0,
  // and SEG_LVL_ATT_LF_Y_H for segment 1.
  pic_hdr.feature_enabled[0][0] = true;
  pic_hdr.feature_enabled[0][1] = true;
  pic_hdr.feature_enabled[0][2] = false;
  pic_hdr.feature_data[0][0] = -67;
  pic_hdr.feature_data[0][1] = 5;
  pic_hdr.feature_enabled[1][2] = true;
  pic_hdr.feature_data[1][2] = -12;
  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data:
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }

  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  EXPECT_TRUE(frame_header.segmentation.enabled);
  EXPECT_TRUE(frame_header.segmentation.update_map);
  EXPECT_FALSE(frame_header.segmentation.temporal_update);
  EXPECT_TRUE(frame_header.segmentation.update_data);
  EXPECT_TRUE(frame_header.segmentation.feature_enabled[0][0]);
  EXPECT_TRUE(frame_header.segmentation.feature_enabled[0][1]);
  EXPECT_FALSE(frame_header.segmentation.feature_enabled[0][2]);
  EXPECT_FALSE(frame_header.segmentation.feature_enabled[1][0]);
  EXPECT_FALSE(frame_header.segmentation.feature_enabled[1][1]);
  EXPECT_TRUE(frame_header.segmentation.feature_enabled[1][2]);
  EXPECT_EQ(frame_header.segmentation.feature_data[0][0],
            pic_hdr.feature_data[0][0]);
  EXPECT_EQ(frame_header.segmentation.feature_data[0][1],
            pic_hdr.feature_data[0][1]);
  EXPECT_EQ(frame_header.segmentation.feature_data[1][2],
            pic_hdr.feature_data[1][2]);
}

TEST_F(AV1BuilderTest, BuildSeqHeaderWithColorConfigProfile0) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  seq_hdr.color_description_present_flag = true;
  seq_hdr.color_primaries = kLibgav1ColorPrimaryBt709;
  seq_hdr.transfer_characteristics = kLibgav1TransferCharacteristicsBt709;
  seq_hdr.matrix_coefficients = kLibgav1MatrixCoefficientsBt709;
  seq_hdr.color_range = kLibgav1ColorRangeStudio;
  seq_hdr.chroma_sample_position = kLibgav1ChromaSamplePositionUnknown;
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto sequence_header = parser->sequence_header();
  EXPECT_EQ(sequence_header.color_config.color_primary,
            kLibgav1ColorPrimaryBt709);
  EXPECT_EQ(sequence_header.color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsBt709);
  EXPECT_EQ(sequence_header.color_config.matrix_coefficients,
            kLibgav1MatrixCoefficientsBt709);
  EXPECT_EQ(sequence_header.color_config.color_range, kLibgav1ColorRangeStudio);
  EXPECT_EQ(sequence_header.color_config.chroma_sample_position,
            kLibgav1ChromaSamplePositionUnknown);
}

TEST_F(AV1BuilderTest, BuildSeqHeaderWithColorConfigProfile1) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  seq_hdr.profile = 1;
  seq_hdr.color_description_present_flag = true;
  seq_hdr.color_primaries = kLibgav1ColorPrimaryBt709;
  seq_hdr.transfer_characteristics = kLibgav1TransferCharacteristicsBt709;
  seq_hdr.matrix_coefficients = kLibgav1MatrixCoefficientsBt709;
  seq_hdr.color_range = kLibgav1ColorRangeStudio;
  seq_hdr.chroma_sample_position = kLibgav1ChromaSamplePositionUnknown;
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto sequence_header = parser->sequence_header();
  EXPECT_EQ(sequence_header.profile, libgav1::kProfile1);
  EXPECT_EQ(sequence_header.color_config.color_primary,
            kLibgav1ColorPrimaryBt709);
  EXPECT_EQ(sequence_header.color_config.transfer_characteristics,
            kLibgav1TransferCharacteristicsBt709);
  EXPECT_EQ(sequence_header.color_config.matrix_coefficients,
            kLibgav1MatrixCoefficientsBt709);
  EXPECT_EQ(sequence_header.color_config.color_range, kLibgav1ColorRangeStudio);
  EXPECT_EQ(sequence_header.color_config.chroma_sample_position,
            kLibgav1ChromaSamplePositionUnknown);
}

TEST_F(AV1BuilderTest, BuildFrameOBUWithQuantizationParams) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  pic_hdr.delta_q_present = false;
  pic_hdr.delta_q_y_dc = -2;
  pic_hdr.delta_q_u_dc = -1;
  pic_hdr.delta_q_u_ac = 0;
  pic_hdr.delta_q_v_dc = 4;
  pic_hdr.delta_q_v_ac = -3;
  pic_hdr.using_qmatrix = true;
  pic_hdr.qm_y = 2;
  pic_hdr.qm_u = 3;
  pic_hdr.qm_v = 4;

  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  EXPECT_EQ(frame_header.quantizer.base_index, 100);
  EXPECT_EQ(frame_header.quantizer.delta_dc[0], pic_hdr.delta_q_y_dc);
  EXPECT_EQ(frame_header.quantizer.delta_dc[1], pic_hdr.delta_q_u_dc);
  EXPECT_EQ(frame_header.quantizer.delta_ac[1], pic_hdr.delta_q_u_ac);
  EXPECT_EQ(frame_header.quantizer.delta_dc[2], pic_hdr.delta_q_v_dc);
  EXPECT_EQ(frame_header.quantizer.delta_ac[2], pic_hdr.delta_q_v_ac);
  EXPECT_TRUE(frame_header.quantizer.use_matrix);
  EXPECT_EQ(frame_header.quantizer.matrix_level[0], pic_hdr.qm_y);
  EXPECT_EQ(frame_header.quantizer.matrix_level[1], pic_hdr.qm_u);
  EXPECT_EQ(frame_header.quantizer.matrix_level[2], pic_hdr.qm_v);
}

TEST_F(AV1BuilderTest, BuildFrameOBUWithLoopFilter) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  pic_hdr.delta_q_present = true;
  pic_hdr.loop_filter_delta_enabled = true;
  pic_hdr.loop_filter_delta_update = true;
  pic_hdr.update_ref_delta = true;
  pic_hdr.loop_filter_ref_deltas = {1, -1, 2, -2, 3, -3, 4, -4};
  pic_hdr.update_mode_delta = true;
  pic_hdr.loop_filter_mode_deltas = {1, -1};
  pic_hdr.delta_lf_present = true;
  pic_hdr.delta_lf_res = 2;
  pic_hdr.delta_lf_multi = true;

  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Create a fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  // Write the tile_group_obu into packed_frame.
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  EXPECT_TRUE(frame_header.loop_filter.delta_enabled);
  EXPECT_TRUE(frame_header.loop_filter.delta_update);
  EXPECT_EQ(frame_header.loop_filter.ref_deltas,
            pic_hdr.loop_filter_ref_deltas);
  EXPECT_TRUE(frame_header.delta_lf.present);
  EXPECT_EQ(frame_header.delta_lf.scale, pic_hdr.delta_lf_res);
  EXPECT_TRUE(frame_header.delta_lf.multi);
}

TEST_F(AV1BuilderTest, BuildFrameOBUWithCDEF) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  seq_hdr.enable_restoration = false;
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  pic_hdr.cdef_damping_minus_3 = 2;
  pic_hdr.cdef_bits = 3;
  for (int i = 0; i < (1 << pic_hdr.cdef_bits); i++) {
    pic_hdr.cdef_y_pri_strength[i] = i;
    pic_hdr.cdef_y_sec_strength[i] = i;
    pic_hdr.cdef_uv_pri_strength[i] = i;
    pic_hdr.cdef_uv_sec_strength[i] = i;
  }

  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  EXPECT_EQ(frame_header.cdef.damping, pic_hdr.cdef_damping_minus_3 + 3);
  EXPECT_EQ(frame_header.cdef.bits, pic_hdr.cdef_bits);
  EXPECT_THAT(pic_hdr.cdef_y_pri_strength,
              ElementsAreArray(frame_header.cdef.y_primary_strength));
}

TEST_F(AV1BuilderTest, BuildFrameOBUWithTxMode) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  seq_hdr.enable_restoration = false;
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  pic_hdr.tx_mode = libgav1::TxMode::kTxModeLargest;

  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  EXPECT_EQ(frame_header.tx_mode, libgav1::TxMode::kTxModeLargest);
}

TEST_F(AV1BuilderTest, BuildFrameOBUWithLoopRestoration) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  seq_hdr.enable_restoration = true;
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  pic_hdr.restoration_type = {
      libgav1::LoopRestorationType::kLoopRestorationTypeWiener,
      libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj,
      libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable};
  pic_hdr.lr_unit_shift = 1;  // Use 128x128 restoration units for Y-plane.
  pic_hdr.lr_uv_shift = 1;    // Use 64x64 restoration units for UV-plane.

  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  EXPECT_EQ(frame_header.loop_restoration.type[0],
            libgav1::LoopRestorationType::kLoopRestorationTypeWiener);
  EXPECT_EQ(frame_header.loop_restoration.unit_size_log2[0], 7);
  EXPECT_EQ(frame_header.loop_restoration.type[1],
            libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj);
  EXPECT_EQ(frame_header.loop_restoration.unit_size_log2[1], 6);
  EXPECT_EQ(frame_header.loop_restoration.type[2],
            libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable);
  EXPECT_EQ(frame_header.loop_restoration.unit_size_log2[2], 6);
}

TEST_F(AV1BuilderTest, BuildFrameOBUWithReferenceSelect) {
  AV1BitstreamBuilder::SequenceHeader seq_hdr = MakeDefaultSequenceHeader();
  seq_hdr.enable_restoration = false;
  seq_hdr.enable_ref_frame_mvs = true;
  AV1BitstreamBuilder seq_header_obu =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(seq_hdr);

  AV1BitstreamBuilder packed_frame;
  packed_frame.WriteOBUHeader(/*type=*/libgav1::kObuTemporalDelimiter,
                              /*has_size=*/true);
  packed_frame.WriteValueInLeb128(0);

  packed_frame.WriteOBUHeader(libgav1::kObuSequenceHeader, /*has_size=*/true);
  EXPECT_EQ(seq_header_obu.OutstandingBits() % 8, 0ull);
  packed_frame.WriteValueInLeb128(seq_header_obu.OutstandingBits() / 8);
  packed_frame.AppendBitstreamBuffer(std::move(seq_header_obu));

  AV1BitstreamBuilder::FrameHeader pic_hdr = MakeFrameHeader(0);
  packed_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr);
  EXPECT_EQ(frame_obu.OutstandingBits() % 8, 0ull);
  // Fake tile_group_obu with only dummy data.
  static const uint8_t tile_group_obu[] = {0x00, 0x80};
  packed_frame.WriteValueInLeb128(frame_obu.OutstandingBits() / 8 +
                                  std::size(tile_group_obu));
  packed_frame.AppendBitstreamBuffer(std::move(frame_obu));
  for (const uint8_t byte : tile_group_obu) {
    packed_frame.Write(byte, 8);
  }
  std::vector<uint8_t> chunk = std::move(packed_frame).Flush();
  auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));

  libgav1::RefCountedBufferPtr current_frame;
  libgav1::StatusCode status = parser->ParseOneFrame(&current_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header = parser->frame_header();
  auto sequence_header = parser->sequence_header();
  av1_decoder_state_->UpdateReferenceFrames(
      current_frame, base::strict_cast<int>(frame_header.refresh_frame_flags));
  EXPECT_FALSE(frame_header.reference_mode_select);

  AV1BitstreamBuilder packed_delta_frame;
  AV1BitstreamBuilder::FrameHeader pic_hdr_delta = MakeFrameHeader(1);
  pic_hdr_delta.reference_select = true;
  pic_hdr_delta.refresh_frame_flags = 0b00000010;
  packed_delta_frame.WriteOBUHeader(libgav1::kObuFrame, /*has_size=*/true);
  AV1BitstreamBuilder frame_obu_delta =
      AV1BitstreamBuilder::BuildFrameHeaderOBU(seq_hdr, pic_hdr_delta);
  EXPECT_EQ(frame_obu_delta.OutstandingBits() % 8, 0ull);

  packed_delta_frame.WriteValueInLeb128(frame_obu_delta.OutstandingBits() / 8 +
                                        std::size(tile_group_obu));
  packed_delta_frame.AppendBitstreamBuffer(std::move(frame_obu_delta));
  for (const uint8_t byte : tile_group_obu) {
    packed_delta_frame.Write(byte, 8);
  }
  chunk = std::move(packed_delta_frame).Flush();
  auto delta_parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
      chunk.data(), chunk.size(), 0, buffer_pool_.get(),
      av1_decoder_state_.get()));
  delta_parser->set_sequence_header(sequence_header);

  libgav1::RefCountedBufferPtr current_delta_frame;
  status = delta_parser->ParseOneFrame(&current_delta_frame);

  EXPECT_EQ(status, libgav1::kStatusOk);
  auto frame_header_delta = delta_parser->frame_header();
  EXPECT_TRUE(frame_header_delta.reference_mode_select);
}

}  // namespace media
