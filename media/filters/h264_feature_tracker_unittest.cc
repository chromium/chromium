// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/h264_feature_tracker.h"

#include <stdint.h>

#include <vector>

#include "base/numerics/byte_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/parsers/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class SimpleNaluWriter {
 public:
  SimpleNaluWriter(H264NALU::Type type, int nal_ref_idc) {
    AppendBits(1, 0);
    AppendBits(2, nal_ref_idc);
    AppendBits(5, type);
  }

  void AppendBits(size_t count, uint32_t value) {
    for (size_t i = count; i > 0; --i) {
      curr_byte_ = (curr_byte_ << 1) | ((value >> (i - 1)) & 1u);
      if (++bits_in_curr_ == 8) {
        rbsp_.push_back(curr_byte_);
        curr_byte_ = 0;
        bits_in_curr_ = 0;
      }
    }
  }

  void AppendBool(bool value) { AppendBits(1, value ? 1u : 0u); }

  void AppendUE(uint32_t value) {
    uint32_t code = value + 1;
    size_t zeros = 0;
    for (uint32_t tmp = code; tmp > 1; tmp >>= 1) {
      ++zeros;
    }
    AppendBits(zeros, 0);
    AppendBits(zeros + 1, code);
  }

  void AppendSE(int32_t value) {
    AppendUE(value > 0 ? static_cast<uint32_t>(value) * 2 - 1
                       : static_cast<uint32_t>(-value) * 2);
  }

  std::vector<uint8_t> Finish(bool with_start_code) {
    AppendBool(true);  // RBSP stop bit.
    if (bits_in_curr_ > 0) {
      AppendBits(8 - bits_in_curr_, 0);
    }

    std::vector<uint8_t> out;
    if (with_start_code) {
      out.insert(out.end(), {0, 0, 0, 1});
    }
    int zero_count = 0;
    for (uint8_t byte : rbsp_) {
      if (zero_count == 2 && byte <= 3) {
        out.push_back(3);
        zero_count = 0;
      }
      out.push_back(byte);
      zero_count = (byte == 0) ? zero_count + 1 : 0;
    }
    return out;
  }

 private:
  std::vector<uint8_t> rbsp_;
  uint8_t curr_byte_ = 0;
  size_t bits_in_curr_ = 0;
};

struct StreamSpec {
  uint8_t profile_idc = 100;
  uint8_t constraint_flags = 0;
  uint32_t chroma_format_idc = 1;
  uint32_t bit_depth_minus8 = 0;
  bool frame_mbs_only_flag = true;
  bool mb_adaptive_frame_field_flag = false;
  bool field_pic_flag = false;
};

std::vector<uint8_t> MakeSps(const StreamSpec& spec, bool annex_b = true) {
  SimpleNaluWriter w(H264NALU::kSPS, 3);
  w.AppendBits(8, spec.profile_idc);
  w.AppendBits(8, spec.constraint_flags);
  w.AppendBits(8, 30);
  w.AppendUE(0);  // sps_id
  if (spec.profile_idc == 44 || spec.profile_idc == 83 ||
      spec.profile_idc == 86 || spec.profile_idc == 100 ||
      spec.profile_idc == 110 || spec.profile_idc == 118 ||
      spec.profile_idc == 122 || spec.profile_idc == 128 ||
      spec.profile_idc == 244) {
    w.AppendUE(spec.chroma_format_idc);
    if (spec.chroma_format_idc == 3) {
      w.AppendBool(false);  // separate_colour_plane_flag
    }
    w.AppendUE(spec.bit_depth_minus8);
    w.AppendUE(spec.bit_depth_minus8);
    w.AppendBool(false);  // qpprime_y_zero_transform_bypass_flag
    w.AppendBool(false);  // seq_scaling_matrix_present_flag
  }
  w.AppendUE(0);        // log2_max_frame_num_minus4
  w.AppendUE(2);        // pic_order_cnt_type
  w.AppendUE(0);        // max_num_ref_frames
  w.AppendBool(false);  // gaps_in_frame_num_value_allowed_flag
  w.AppendUE(0);        // pic_width_in_mbs_minus1
  w.AppendUE(0);        // pic_height_in_map_units_minus1
  w.AppendBool(spec.frame_mbs_only_flag);
  if (!spec.frame_mbs_only_flag) {
    w.AppendBool(spec.mb_adaptive_frame_field_flag);
  }
  w.AppendBool(true);   // direct_8x8_inference_flag
  w.AppendBool(false);  // frame_cropping_flag
  w.AppendBool(false);  // vui_parameters_present_flag
  return w.Finish(annex_b);
}

std::vector<uint8_t> MakePps(bool annex_b = true,
                             uint32_t num_slice_groups_minus1 = 0) {
  SimpleNaluWriter w(H264NALU::kPPS, 3);
  w.AppendUE(0);                        // pps_id
  w.AppendUE(0);                        // sps_id
  w.AppendBool(false);                  // entropy_coding_mode_flag
  w.AppendBool(false);                  // bottom_field_pic_order_in_frame
  w.AppendUE(num_slice_groups_minus1);  // num_slice_groups_minus1
  w.AppendUE(0);                        // num_ref_idx_l0_default_active_minus1
  w.AppendUE(0);                        // num_ref_idx_l1_default_active_minus1
  w.AppendBool(false);                  // weighted_pred_flag
  w.AppendBits(2, 0);                   // weighted_bipred_idc
  w.AppendSE(0);                        // pic_init_qp_minus26
  w.AppendSE(0);                        // pic_init_qs_minus26
  w.AppendSE(0);                        // chroma_qp_index_offset
  w.AppendBool(false);                  // deblocking_filter_control_present
  w.AppendBool(false);                  // constrained_intra_pred_flag
  w.AppendBool(false);                  // redundant_pic_cnt_present_flag
  return w.Finish(annex_b);
}

std::vector<uint8_t> MakeSlice(const StreamSpec& spec, bool annex_b = true) {
  SimpleNaluWriter w(H264NALU::kIDRSlice, 3);
  w.AppendUE(0);       // first_mb_in_slice
  w.AppendUE(7);       // slice_type = I
  w.AppendUE(0);       // pic_parameter_set_id
  w.AppendBits(4, 0);  // frame_num
  if (!spec.frame_mbs_only_flag) {
    w.AppendBool(spec.field_pic_flag);
    if (spec.field_pic_flag) {
      w.AppendBool(false);  // bottom_field_flag
    }
  }
  w.AppendUE(0);        // idr_pic_id
  w.AppendBool(false);  // no_output_of_prior_pics_flag
  w.AppendBool(false);  // long_term_reference_flag
  w.AppendSE(0);        // slice_qp_delta
  return w.Finish(annex_b);
}

std::vector<uint8_t> MakeAnnexBPacket(const StreamSpec& spec,
                                      uint32_t num_slice_groups_minus1 = 0) {
  std::vector<uint8_t> packet;
  for (const auto& nalu :
       {MakeSps(spec), MakePps(/*annex_b=*/true, num_slice_groups_minus1),
        MakeSlice(spec)}) {
    packet.insert(packet.end(), nalu.begin(), nalu.end());
  }
  return packet;
}

std::vector<uint8_t> MakeAvcConfig(const StreamSpec& spec,
                                   uint8_t length_size) {
  mp4::AVCDecoderConfigurationRecord avc_config;
  avc_config.version = 1;
  avc_config.profile_indication = spec.profile_idc;
  avc_config.profile_compatibility = spec.constraint_flags;
  avc_config.avc_level = 30;
  avc_config.length_size = length_size;
  avc_config.chroma_format = spec.chroma_format_idc;
  avc_config.bit_depth_luma_minus8 = spec.bit_depth_minus8;
  avc_config.bit_depth_chroma_minus8 = spec.bit_depth_minus8;
  avc_config.sps_list.push_back(MakeSps(spec, /*annex_b=*/false));
  avc_config.pps_list.push_back(MakePps(/*annex_b=*/false));
  std::vector<uint8_t> avcc;
  EXPECT_TRUE(avc_config.Serialize(avcc));
  return avcc;
}

std::vector<uint8_t> MakeAvcPacket(base::span<const uint8_t> nalu,
                                   uint8_t length_size) {
  const auto len_be = base::U32ToBigEndian(static_cast<uint32_t>(nalu.size()));
  const auto len_prefix = base::span(len_be).last(length_size);
  std::vector<uint8_t> packet(len_prefix.begin(), len_prefix.end());
  packet.insert(packet.end(), nalu.begin(), nalu.end());
  return packet;
}

void ExpectMetrics(const base::HistogramTester& histograms,
                   VideoChromaSampling chroma,
                   int depth,
                   H264Interlacing interlacing,
                   VideoCodecProfile profile) {
  histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.Conformant",
                                true, 1);
  histograms.ExpectUniqueSample(
      "Media.FFmpegVideoDecoder.H264.ChromaSubsampling", chroma, 1);
  histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.ColorDepth",
                                depth, 1);
  histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.Interlacing",
                                interlacing, 1);
  histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.Profile",
                                profile, 1);
}

}  // namespace

TEST(H264FeatureTrackerTest, DetectsFeaturesAndProfiles) {
  struct Case {
    StreamSpec spec;
    VideoChromaSampling chroma = VideoChromaSampling::k420;
    int depth = 8;
    H264Interlacing interlacing = H264Interlacing::kProgressive;
    VideoCodecProfile profile = H264PROFILE_HIGH;
  };
  const Case kCases[] = {
      {{.profile_idc = 66},
       VideoChromaSampling::k420,
       8,
       H264Interlacing::kProgressive,
       H264PROFILE_BASELINE},
      {{.profile_idc = 88},
       VideoChromaSampling::k420,
       8,
       H264Interlacing::kProgressive,
       H264PROFILE_EXTENDED},
      {{.profile_idc = 110, .bit_depth_minus8 = 2},
       VideoChromaSampling::k420,
       10,
       H264Interlacing::kProgressive,
       H264PROFILE_HIGH10PROFILE},
      {{.profile_idc = 244, .bit_depth_minus8 = 4},
       VideoChromaSampling::k420,
       12,
       H264Interlacing::kProgressive,
       H264PROFILE_HIGH444PREDICTIVEPROFILE},
      {{.profile_idc = 122, .chroma_format_idc = 2},
       VideoChromaSampling::k422,
       8,
       H264Interlacing::kProgressive,
       H264PROFILE_HIGH422PROFILE},
      {{.profile_idc = 244, .chroma_format_idc = 3},
       VideoChromaSampling::k444,
       8,
       H264Interlacing::kProgressive,
       H264PROFILE_HIGH444PREDICTIVEPROFILE},
      {{.profile_idc = 100, .chroma_format_idc = 0},
       VideoChromaSampling::k400,
       8,
       H264Interlacing::kProgressive,
       H264PROFILE_HIGH},
      {{.profile_idc = 77,
        .frame_mbs_only_flag = false,
        .field_pic_flag = true},
       VideoChromaSampling::k420,
       8,
       H264Interlacing::kPaff,
       H264PROFILE_MAIN},
      {{.profile_idc = 77,
        .frame_mbs_only_flag = false,
        .mb_adaptive_frame_field_flag = true},
       VideoChromaSampling::k420,
       8,
       H264Interlacing::kMbaff,
       H264PROFILE_MAIN},
      // Progressive frame in a PAFF-capable SPS must not report PAFF.
      {{.profile_idc = 77, .frame_mbs_only_flag = false},
       VideoChromaSampling::k420,
       8,
       H264Interlacing::kProgressive,
       H264PROFILE_MAIN},
  };

  for (const auto& c : kCases) {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParsePacket(MakeAnnexBPacket(c.spec));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();
    ExpectMetrics(histograms, c.chroma, c.depth, c.interlacing, c.profile);
  }
}

TEST(H264FeatureTrackerTest, InterlacingBothAndAvcConfig) {
  // Both MBAFF and PAFF in the same session report H264Interlacing::kBoth.
  {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    StreamSpec mbaff_and_paff{
        .profile_idc = 77,
        .frame_mbs_only_flag = false,
        .mb_adaptive_frame_field_flag = true,
        .field_pic_flag = false,
    };
    tracker.ParsePacket(MakeAnnexBPacket(mbaff_and_paff));
    mbaff_and_paff.field_pic_flag = true;
    tracker.ParsePacket(MakeSlice(mbaff_and_paff));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();
    ExpectMetrics(histograms, VideoChromaSampling::k420, 8,
                  H264Interlacing::kBoth, H264PROFILE_MAIN);
  }

  // avcC extradata with length sizes 1, 2, and 4.
  const StreamSpec spec{
      .profile_idc = 122,
      .constraint_flags = 0x10,
      .chroma_format_idc = 2,
      .bit_depth_minus8 = 2,
  };
  const auto raw_slice = MakeSlice(spec, /*annex_b=*/false);
  for (uint8_t length_size : {1, 2, 4}) {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParseExtraData(MakeAvcConfig(spec, length_size));
    tracker.ParsePacket(MakeAvcPacket(raw_slice, length_size));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();
    tracker.ReportMetrics();  // No-op on repeat call.

    ExpectMetrics(histograms, VideoChromaSampling::k422, 10,
                  H264Interlacing::kProgressive, H264PROFILE_HIGH422PROFILE);
  }

  // avcC extradata followed by Annex-B packets (including 3-byte start code
  // `00 00 01 xx` whose first 4 bytes look like a 256..511 length <= packet
  // size), then switching back to AVC.
  {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParseExtraData(MakeAvcConfig(spec, /*length_size=*/4));

    std::vector<uint8_t> annex_b_3byte = {0, 0, 1};
    annex_b_3byte.insert(annex_b_3byte.end(), raw_slice.begin(),
                         raw_slice.end());
    annex_b_3byte.resize(400, 0xFF);  // > 0x0165 (357) bytes.
    tracker.ParsePacket(annex_b_3byte);
    tracker.ParsePacket(MakeSlice(spec, /*annex_b=*/true));
    tracker.ParsePacket(MakeAvcPacket(raw_slice, /*length_size=*/4));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();

    ExpectMetrics(histograms, VideoChromaSampling::k422, 10,
                  H264Interlacing::kProgressive, H264PROFILE_HIGH422PROFILE);
  }

  // Annex-B extradata followed by an Annex-B slice packet.
  {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    std::vector<uint8_t> extra = MakeSps(spec);
    const auto pps = MakePps();
    extra.insert(extra.end(), pps.begin(), pps.end());
    tracker.ParseExtraData(extra);
    tracker.ParsePacket(MakeSlice(spec));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();

    ExpectMetrics(histograms, VideoChromaSampling::k422, 10,
                  H264Interlacing::kProgressive, H264PROFILE_HIGH422PROFILE);
  }

  // avc3-style avcC with zero SPS/PPS followed by in-band AVC parameter sets.
  {
    mp4::AVCDecoderConfigurationRecord empty_config;
    empty_config.version = 1;
    empty_config.profile_indication = spec.profile_idc;
    empty_config.profile_compatibility = spec.constraint_flags;
    empty_config.avc_level = 30;
    empty_config.length_size = 4;
    empty_config.chroma_format = spec.chroma_format_idc;
    empty_config.bit_depth_luma_minus8 = spec.bit_depth_minus8;
    empty_config.bit_depth_chroma_minus8 = spec.bit_depth_minus8;
    std::vector<uint8_t> avcc;
    ASSERT_TRUE(empty_config.Serialize(avcc));

    std::vector<uint8_t> packet;
    for (const auto& nalu : {MakeSps(spec, /*annex_b=*/false),
                             MakePps(/*annex_b=*/false), raw_slice}) {
      const auto avc_nalu = MakeAvcPacket(nalu, /*length_size=*/4);
      packet.insert(packet.end(), avc_nalu.begin(), avc_nalu.end());
    }

    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParseExtraData(avcc);
    tracker.ParsePacket(packet);
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();

    ExpectMetrics(histograms, VideoChromaSampling::k422, 10,
                  H264Interlacing::kProgressive, H264PROFILE_HIGH422PROFILE);
  }
}

TEST(H264FeatureTrackerTest, NoFramesAndNonConformantStreams) {
  // No frames decoded: nothing reported.
  {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParsePacket(MakeAnnexBPacket({}));
    tracker.ReportMetrics();
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Conformant", 0);
    histograms.ExpectTotalCount(
        "Media.FFmpegVideoDecoder.H264.ChromaSubsampling", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.ColorDepth", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Interlacing", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Profile", 0);
  }
  // H264Parser rejects invalid SPS (`bit_depth_minus8 == 7`), but decoder
  // still outputs a frame: reports Conformant=false and omits detail UMAs.
  {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParsePacket(
        MakeAnnexBPacket({.profile_idc = 244, .bit_depth_minus8 = 7}));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();
    histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.Conformant",
                                  false, 1);
    histograms.ExpectTotalCount(
        "Media.FFmpegVideoDecoder.H264.ChromaSubsampling", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.ColorDepth", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Interlacing", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Profile", 0);
  }
  // Valid packet followed by a parse error (e.g., forbidden_zero_bit=1) still
  // marks the session non-conformant.
  {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParsePacket(MakeAnnexBPacket({}));
    const uint8_t kInvalidNalu[] = {0, 0, 0, 1, 0x85, 0x00};
    tracker.ParsePacket(kInvalidNalu);
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();
    histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.Conformant",
                                  false, 1);
    histograms.ExpectTotalCount(
        "Media.FFmpegVideoDecoder.H264.ChromaSubsampling", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.ColorDepth", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Interlacing", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Profile", 0);
  }
  // FMO slice groups (both 2 and >2 slice groups) are unsupported by
  // H264Parser and consistently report Conformant=false.
  for (uint32_t num_slice_groups_minus1 : {1u, 2u}) {
    base::HistogramTester histograms;
    H264FeatureTracker tracker;
    tracker.ParsePacket(
        MakeAnnexBPacket({.profile_idc = 66}, num_slice_groups_minus1));
    tracker.OnFrameDecoded();
    tracker.ReportMetrics();
    histograms.ExpectUniqueSample("Media.FFmpegVideoDecoder.H264.Conformant",
                                  false, 1);
    histograms.ExpectTotalCount(
        "Media.FFmpegVideoDecoder.H264.ChromaSubsampling", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.ColorDepth", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Interlacing", 0);
    histograms.ExpectTotalCount("Media.FFmpegVideoDecoder.H264.Profile", 0);
  }
}

}  // namespace media
