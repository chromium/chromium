// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/video/h266_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
struct VvcTestData {
  std::string file_name;
  // Number of NALUs in the test stream to be parsed.
  int num_nalus;
};

}  // namespace

class H266ParserTest : public ::testing::Test {
 protected:
  void LoadParserFile(std::string file_name) {
    parser_.Reset();
    base::FilePath file_path = GetTestDataFilePath(file_name);
    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
    parser_.SetStream(stream_->data(), stream_->length());
  }
  bool ParseNalusUntilNut(H266NALU* target_nalu, H266NALU::Type nalu_type) {
    while (true) {
      H266Parser::Result res = parser_.AdvanceToNextNALU(target_nalu);
      if (res == H266Parser::kEndOfStream) {
        return false;
      }
      EXPECT_EQ(res, H266Parser::kOk);
      if (target_nalu->nal_unit_type == nalu_type) {
        return true;
      }
    }
  }
  H266Parser parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;
};

TEST_F(H266ParserTest, RawVvcStreamFileParsingShouldSucceed) {
  VvcTestData test_data[] = {{"bear_180p.vvc", 54},
                             {"bbb_360p.vvc", 87},
                             {"basketball_2_layers.vvc", 48}};
  for (const auto& data : test_data) {
    LoadParserFile(data.file_name);
    // Parse until the end of stream/unsupported stream/error in stream is
    // found.
    int num_parsed_nalus = 0;
    while (true) {
      H266NALU nalu;
      H266Parser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H266Parser::kEndOfStream) {
        DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
                 << num_parsed_nalus;
        EXPECT_EQ(data.num_nalus, num_parsed_nalus);
        break;
      }
      EXPECT_EQ(res, H266Parser::kOk);
      ++num_parsed_nalus;
      DVLOG(4) << "Found NALU " << nalu.nal_unit_type;
      switch (nalu.nal_unit_type) {
        case H266NALU::kVPS:
          int vps_id;
          res = parser_.ParseVPS(&vps_id);
          EXPECT_TRUE(!!parser_.GetVPS(vps_id));
          break;
        // TODO(crbugs.com/1417910): add more NALU types.
        default:
          break;
      }
      EXPECT_EQ(res, H266Parser::kOk);
    }
  }
}

TEST_F(H266ParserTest, VpsWithTwolayersParsingShouldSucceed) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kVPS));
  int vps_id;
  EXPECT_EQ(H266Parser::kOk, parser_.ParseVPS(&vps_id));
  const H266VPS* vps = parser_.GetVPS(vps_id);
  EXPECT_TRUE(!!vps);
  EXPECT_EQ(vps->vps_video_parameter_set_id, 1);
  EXPECT_EQ(vps->vps_max_layers_minus1, 1);
  EXPECT_EQ(vps->vps_max_sublayers_minus1, 0);
  for (int i = 0; i <= vps->vps_max_layers_minus1; i++) {
    EXPECT_EQ(vps->vps_layer_id[i], i);
  }
  EXPECT_TRUE(vps->vps_direct_ref_layer_flag[1][0]);
  EXPECT_EQ(vps->vps_ols_mode_idc, 2);
  EXPECT_TRUE(vps->vps_ols_output_layer_flag[1][0]);
  EXPECT_TRUE(vps->vps_ols_output_layer_flag[1][1]);
  EXPECT_EQ(vps->vps_num_ptls_minus1, 1);
  EXPECT_TRUE(vps->vps_pt_present_flag[0]);
  // vps->vps_pt_present_flag[1] = 0, so profile_tier_level[1] should
  // be copied from profile_tier_level[0].
  for (int i = 0; i <= vps->vps_num_ptls_minus1; i++) {
    EXPECT_EQ(vps->profile_tier_level[i].general_profile_idc, 17);
    EXPECT_EQ(vps->profile_tier_level[i].general_level_idc, 51);
    EXPECT_TRUE(vps->profile_tier_level[i].ptl_frame_only_constraint_flag);
  }
  EXPECT_EQ(vps->vps_num_dpb_params_minus1, 0);
  EXPECT_EQ(vps->dpb_parameters[0].dpb_max_dec_pic_buffering_minus1[0], 9);
  EXPECT_EQ(vps->dpb_parameters[0].dpb_max_num_reorder_pics[0], 9);
  EXPECT_EQ(vps->dpb_parameters[0].dpb_max_latency_increase_plus1[0], 0);
  EXPECT_EQ(vps->vps_ols_dpb_pic_width[0], 832);
  EXPECT_EQ(vps->vps_ols_dpb_pic_height[0], 480);
  EXPECT_EQ(vps->vps_ols_dpb_chroma_format[0], 1);
  EXPECT_EQ(vps->vps_ols_dpb_bitdepth_minus8[0], 2);
}

TEST_F(H266ParserTest, GetVPSForStreamWithoutVPSShouldReturnNull) {
  LoadParserFile("bear_180p.vvc");
  H266NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!vps);
}

TEST_F(H266ParserTest, GetVPSWithoutVPSParsingShouldReturnNull) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  const H266VPS* vps = parser_.GetVPS(1);
  EXPECT_TRUE(!vps);
}

}  // namespace media
