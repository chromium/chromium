// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_builder.h"

#include "base/logging.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/temporal_scalability_id_extractor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class H264BuilderTest : public ::testing::Test {
 public:
  // bear.mp4 without VUI parameters.
  static H264SPS MakeTestSPS() {
    H264SPS sps;
    sps.profile_idc = 100;
    sps.level_idc = 13;
    sps.chroma_format_idc = 1;
    for (auto& row : sps.scaling_list4x4) {
      row.fill(16u);
    }
    for (auto& row : sps.scaling_list8x8) {
      row.fill(16u);
    }
    sps.log2_max_frame_num_minus4 = 5;
    sps.log2_max_pic_order_cnt_lsb_minus4 = 6;
    sps.max_num_ref_frames = 4;
    sps.pic_width_in_mbs_minus1 = 19;
    sps.pic_height_in_map_units_minus1 = 11;
    sps.frame_mbs_only_flag = true;
    sps.direct_8x8_inference_flag = true;
    sps.frame_cropping_flag = true;
    sps.frame_crop_bottom_offset = 6;
    sps.chroma_array_type = 1;
    return sps;
  }

  static H264PPS MakeTestPPS() {
    H264PPS pps;
    pps.entropy_coding_mode_flag = true;
    pps.weighted_bipred_idc = 2;
    pps.chroma_qp_index_offset = -2;
    pps.deblocking_filter_control_present_flag = true;
    pps.transform_8x8_mode_flag = true;
    pps.second_chroma_qp_index_offset = -2;
    return pps;
  }

  static void TestPrefixNALU(int num_temporal_layers,
                             size_t nal_ref_idc,
                             H264NALU::Type associated_nalu_type,
                             uint8_t temporal_id) {
    const std::vector<uint8_t> prefix_nalu =
        BuildPrefixNALU(nal_ref_idc, associated_nalu_type, temporal_id);
    ASSERT_FALSE(prefix_nalu.empty());

    TemporalScalabilityIdExtractor temporal_id_scalability_id_extractor(
        VideoCodec::kH264, num_temporal_layers);
    TemporalScalabilityIdExtractor::BitstreamMetadata metadata;
    EXPECT_TRUE(temporal_id_scalability_id_extractor.ParseChunk(prefix_nalu,
                                                                /*frame_id=*/0,
                                                                metadata));
    EXPECT_EQ(metadata.temporal_id, static_cast<int>(temporal_id));
  }
};

TEST_F(H264BuilderTest, H264BuildParseIdentity) {
  H264SPS sps = MakeTestSPS();
  H264PPS pps = MakeTestPPS();

  H26xAnnexBBitstreamBuilder bitstream_builder(
      /*insert_emulation_prevention_bytes=*/true);
  BuildPackedH264SPS(bitstream_builder, sps);
  BuildPackedH264PPS(bitstream_builder, sps, pps);

  H264Parser parser;
  parser.SetStream(bitstream_builder.data());
  H264NALU nalu;
  EXPECT_EQ(parser.AdvanceToNextNALU(&nalu), H264Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H264NALU::kSPS);
  int sps_id;
  EXPECT_EQ(parser.ParseSPS(&sps_id), H264Parser::Result::kOk);
  EXPECT_EQ(*parser.GetSPS(sps_id), sps);

  EXPECT_EQ(parser.AdvanceToNextNALU(&nalu), H264Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H264NALU::kPPS);
  int pps_id;
  EXPECT_EQ(parser.ParsePPS(&pps_id), H264Parser::Result::kOk);
  EXPECT_EQ(*parser.GetPPS(pps_id), pps);
}

TEST_F(H264BuilderTest, H264BuildPrefixNALU) {
  struct PrefixNALUParams {
    int nal_ref_idc;
    H264NALU::Type associated_nalu_type;
    uint8_t temporal_id;
  };

  auto generateParams = [](uint8_t num_temporal_layers) {
    std::vector<PrefixNALUParams> params;
    constexpr bool kIsRef[] = {false, true};
    constexpr bool kIsIdr[] = {false, true};
    for (bool is_ref : kIsRef) {
      for (bool is_idr : kIsIdr) {
        for (uint8_t tid = 0; tid < num_temporal_layers; tid++) {
          // Filter invalid patterns.
          if (is_idr && tid != 0) {
            // If the frame is IDR, then it must be the bottom temporal layer.
            continue;
          }
          if (is_ref == (tid == num_temporal_layers - 1)) {
            // The highest temporal layer must not be reference frame.
            // The other temporal layer must be reference frame.
            continue;
          }

          params.push_back(PrefixNALUParams{
              .nal_ref_idc = is_idr ? 3 : is_ref,
              .associated_nalu_type = is_idr ? H264NALU::Type::kIDRSlice
                                             : H264NALU::Type::kNonIDRSlice,
              .temporal_id = tid,
          });
        }
      }
    }
    return params;
  };

  auto l1t2_params = generateParams(2);
  for (const auto& param : l1t2_params) {
    TestPrefixNALU(2, param.nal_ref_idc, param.associated_nalu_type,
                   param.temporal_id);
  }

  auto l1t3_params = generateParams(3);
  for (const auto& param : l1t3_params) {
    TestPrefixNALU(3, param.nal_ref_idc, param.associated_nalu_type,
                   param.temporal_id);
  }
}

TEST_F(H264BuilderTest, BuildHDRStaticMetadataSEI) {
  H26xSEIMasteringDisplayInfo mdcv;
  mdcv.display_primaries[0] = {13250, 34500};  // G
  mdcv.display_primaries[1] = {7500, 3000};    // B
  mdcv.display_primaries[2] = {34000, 16000};  // R
  mdcv.white_points = {15635, 16450};
  mdcv.max_luminance = 10000000;
  // Contains a 0x000001 sequence, exercising emulation prevention.
  mdcv.min_luminance = 500;

  H26xSEIContentLightLevelInfo clli;
  clli.max_content_light_level = 1000;
  clli.max_picture_average_light_level = 400;

  H26xAnnexBBitstreamBuilder builder(
      /*insert_emulation_prevention_bytes=*/true);
  BuildPackedH264SEI(builder, mdcv, clli);
  builder.Flush();

  H264Parser parser;
  parser.SetStream(builder.data());
  H264NALU nalu;
  ASSERT_EQ(H264Parser::kOk, parser.AdvanceToNextNALU(&nalu));
  ASSERT_EQ(H264NALU::kSEIMessage, nalu.nal_unit_type);

  H264SEI sei;
  ASSERT_EQ(H264Parser::kOk, parser.ParseSEI(&sei));
  bool found_mdcv = false;
  bool found_clli = false;
  for (const auto& sei_msg : sei.msgs) {
    if (const auto* info = std::get_if<H26xSEIMasteringDisplayInfo>(&sei_msg)) {
      found_mdcv = true;
      EXPECT_EQ(info->display_primaries, mdcv.display_primaries);
      EXPECT_EQ(info->white_points, mdcv.white_points);
      EXPECT_EQ(info->max_luminance, mdcv.max_luminance);
      EXPECT_EQ(info->min_luminance, mdcv.min_luminance);
    } else if (const auto* clli_info =
                   std::get_if<H26xSEIContentLightLevelInfo>(&sei_msg)) {
      found_clli = true;
      EXPECT_EQ(clli_info->max_content_light_level,
                clli.max_content_light_level);
      EXPECT_EQ(clli_info->max_picture_average_light_level,
                clli.max_picture_average_light_level);
    }
  }
  EXPECT_TRUE(found_mdcv);
  EXPECT_TRUE(found_clli);
}

}  // namespace media
