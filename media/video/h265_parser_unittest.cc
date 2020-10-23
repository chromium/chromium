// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/video/h265_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
struct HevcTestData {
  std::string file_name;
  // Number of NALUs in the test stream to be parsed.
  int num_nalus;
};

}  // namespace

class H265ParserTest : public ::testing::Test {
 protected:
  void LoadParserFile(std::string file_name) {
    parser_.Reset();
    base::FilePath file_path = GetTestDataFilePath(file_name);

    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();

    parser_.SetStream(stream_->data(), stream_->length());
  }

  bool ParseNalusUntilNut(H265NALU::Type nalu_type) {
    while (true) {
      H265NALU nalu;
      H265Parser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H265Parser::kEOStream) {
        return false;
      }
      EXPECT_EQ(res, H265Parser::kOk);
      if (nalu.nal_unit_type == nalu_type)
        return true;
    }
  }

  H265Parser parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;
};

TEST_F(H265ParserTest, RawHevcStreamFileParsing) {
  HevcTestData test_data[] = {
      {"bear.hevc", 35},
      {"bbb.hevc", 64},
  };

  for (auto& data : test_data) {
    LoadParserFile(data.file_name);
    // Parse until the end of stream/unsupported stream/error in stream is
    // found.
    int num_parsed_nalus = 0;
    while (true) {
      H265NALU nalu;
      H265Parser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H265Parser::kEOStream) {
        DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
                 << num_parsed_nalus;
        EXPECT_EQ(data.num_nalus, num_parsed_nalus);
        break;
      }
      EXPECT_EQ(res, H265Parser::kOk);

      ++num_parsed_nalus;
      DVLOG(4) << "Found NALU " << nalu.nal_unit_type;

      switch (nalu.nal_unit_type) {
        case H265NALU::SPS_NUT:
          int sps_id;
          res = parser_.ParseSPS(&sps_id);
          EXPECT_TRUE(!!parser_.GetSPS(sps_id));
          break;
        default:
          break;
      }
      EXPECT_EQ(res, H265Parser::kOk);
    }
  }
}

TEST_F(H265ParserTest, SpsParsing) {
  LoadParserFile("bear.hevc");
  EXPECT_TRUE(ParseNalusUntilNut(H265NALU::SPS_NUT));
  int sps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSPS(&sps_id));
  const H265SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  EXPECT_EQ(sps->sps_max_sub_layers_minus1, 0);
  EXPECT_EQ(sps->profile_tier_level.general_profile_idc, 1);
  EXPECT_EQ(sps->profile_tier_level.general_level_idc, 60);
  EXPECT_EQ(sps->sps_seq_parameter_set_id, 0);
  EXPECT_EQ(sps->chroma_format_idc, 1);
  EXPECT_FALSE(sps->separate_colour_plane_flag);
  EXPECT_EQ(sps->pic_width_in_luma_samples, 320);
  EXPECT_EQ(sps->pic_height_in_luma_samples, 184);
  EXPECT_EQ(sps->conf_win_left_offset, 0);
  EXPECT_EQ(sps->conf_win_right_offset, 0);
  EXPECT_EQ(sps->conf_win_top_offset, 0);
  EXPECT_EQ(sps->conf_win_bottom_offset, 2);
  EXPECT_EQ(sps->bit_depth_luma_minus8, 0);
  EXPECT_EQ(sps->bit_depth_chroma_minus8, 0);
  EXPECT_EQ(sps->log2_max_pic_order_cnt_lsb_minus4, 4);
  EXPECT_EQ(sps->sps_max_dec_pic_buffering_minus1[0], 4);
  EXPECT_EQ(sps->sps_max_num_reorder_pics[0], 2);
  EXPECT_EQ(sps->sps_max_latency_increase_plus1[0], 0);
  for (int i = 1; i < kMaxSubLayers; ++i) {
    EXPECT_EQ(sps->sps_max_dec_pic_buffering_minus1[i], 0);
    EXPECT_EQ(sps->sps_max_num_reorder_pics[i], 0);
    EXPECT_EQ(sps->sps_max_latency_increase_plus1[i], 0);
  }
  EXPECT_EQ(sps->log2_min_luma_coding_block_size_minus3, 0);
  EXPECT_EQ(sps->log2_diff_max_min_luma_coding_block_size, 3);
  EXPECT_EQ(sps->log2_min_luma_transform_block_size_minus2, 0);
  EXPECT_EQ(sps->log2_diff_max_min_luma_transform_block_size, 3);
  EXPECT_EQ(sps->max_transform_hierarchy_depth_inter, 0);
  EXPECT_EQ(sps->max_transform_hierarchy_depth_intra, 0);
  EXPECT_FALSE(sps->scaling_list_enabled_flag);
  EXPECT_FALSE(sps->sps_scaling_list_data_present_flag);
  EXPECT_FALSE(sps->amp_enabled_flag);
  EXPECT_TRUE(sps->sample_adaptive_offset_enabled_flag);
  EXPECT_FALSE(sps->pcm_enabled_flag);
  EXPECT_EQ(sps->pcm_sample_bit_depth_luma_minus1, 0);
  EXPECT_EQ(sps->pcm_sample_bit_depth_chroma_minus1, 0);
  EXPECT_EQ(sps->log2_min_pcm_luma_coding_block_size_minus3, 0);
  EXPECT_EQ(sps->log2_diff_max_min_pcm_luma_coding_block_size, 0);
  EXPECT_FALSE(sps->pcm_loop_filter_disabled_flag);
  EXPECT_EQ(sps->num_short_term_ref_pic_sets, 0);
  EXPECT_FALSE(sps->long_term_ref_pics_present_flag);
  EXPECT_EQ(sps->num_long_term_ref_pics_sps, 0);
  EXPECT_TRUE(sps->sps_temporal_mvp_enabled_flag);
  EXPECT_TRUE(sps->strong_intra_smoothing_enabled_flag);
}

}  // namespace media
