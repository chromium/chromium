// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/av1_vaapi_video_encoder_delegate.h"

#include <bit>
#include <utility>

#include "base/bits.h"
#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libaom/source/libaom/av1/ratectrl_rtc.h"
#include "third_party/libgav1/src/src/utils/constants.h"

namespace media {
namespace {

// Values from
// third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc
constexpr int kKFPeriod = 3000;

// Quantization parameter. They are av1 ac/dc indices and their ranges are
// 0-255. These are based on WebRTC's defaults.
constexpr uint8_t kMinQP = 40;
constexpr uint8_t kMaxQP = 224;

// This needs to be 64, not 16, because of superblocks.
// TODO: Look into whether or not we can reduce alignment to 16.
constexpr gfx::Size kAV1AlignmentSize(64, 64);
constexpr int kCDEFStrengthDivisor = 4;
constexpr int kPrimaryReferenceNone = 7;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

// Convert Qindex, whose range is 0-255, to the quantizer parameter used in
// libaom av1 rate control, whose range is 0-63.
// The table is generated from the table of
// ited from //third_party/libaom/source/libaom/av1/encoder/av1_quantize.c.
uint8_t QindexToQuantizer(uint8_t q_index) {
  constexpr static uint8_t kQindexToQuantizer[] = {
      0,  1,  1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  4,  5,
      5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,
      9,  10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14,
      14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18,
      18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 23,
      23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 26, 27, 27, 27,
      27, 28, 28, 28, 28, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31, 32,
      32, 32, 32, 33, 33, 33, 33, 34, 34, 34, 34, 35, 35, 35, 35, 36, 36, 36,
      36, 37, 37, 37, 37, 38, 38, 38, 38, 39, 39, 39, 39, 40, 40, 40, 40, 41,
      41, 41, 41, 42, 42, 42, 42, 43, 43, 43, 43, 44, 44, 44, 44, 45, 45, 45,
      45, 46, 46, 46, 46, 47, 47, 47, 47, 48, 48, 48, 48, 49, 49, 49, 49, 50,
      50, 50, 50, 51, 51, 51, 51, 52, 52, 52, 52, 53, 53, 53, 53, 54, 54, 54,
      54, 55, 55, 55, 55, 56, 56, 56, 56, 57, 57, 57, 57, 58, 58, 58, 58, 59,
      59, 59, 59, 60, 60, 60, 60, 61, 61, 61, 61, 62, 62, 62, 62, 62, 63, 63,
      63, 63, 63, 63,
  };
  static_assert(std::size(kQindexToQuantizer) == 256,
                "Unexpected kQindexToQuantizer size");
  CHECK_LT(base::strict_cast<size_t>(q_index), std::size(kQindexToQuantizer));
  return kQindexToQuantizer[q_index];
}

// TODO: Do we need other reference modes?
enum AV1ReferenceMode {
  kSingleReference = 0,
  kCompoundReference = 1,
  kReferenceModeSelect = 2,
};

struct {
  int level_idx;
  int max_width;
  int max_height;
  uint64_t max_sample_rate;
} kAV1LevelSpecs[] = {
    {
        .level_idx = 0,
        .max_width = 2048,
        .max_height = 1152,
        .max_sample_rate = 5529600,
    },
    {
        .level_idx = 1,
        .max_width = 2816,
        .max_height = 1152,
        .max_sample_rate = 10454400,
    },
    {
        .level_idx = 4,
        .max_width = 4352,
        .max_height = 2448,
        .max_sample_rate = 24969600,
    },
    {
        .level_idx = 5,
        .max_width = 5504,
        .max_height = 3096,
        .max_sample_rate = 39938400,
    },
    {
        .level_idx = 8,
        .max_width = 6144,
        .max_height = 3456,
        .max_sample_rate = 77856768,
    },
    {
        .level_idx = 9,
        .max_width = 6144,
        .max_height = 3456,
        .max_sample_rate = 155713536,
    },
    {
        .level_idx = 12,
        .max_width = 8192,
        .max_height = 4352,
        .max_sample_rate = 273715200,
    },
    {
        .level_idx = 13,
        .max_width = 8192,
        .max_height = 4352,
        .max_sample_rate = 547430400,
    },
    {
        .level_idx = 14,
        .max_width = 8192,
        .max_height = 4352,
        .max_sample_rate = 1094860800,
    },
    {
        .level_idx = 15,
        .max_width = 8192,
        .max_height = 4352,
        .max_sample_rate = 1176502272,
    },
    {
        .level_idx = 16,
        .max_width = 16384,
        .max_height = 8704,
        .max_sample_rate = 1176502272,
    },
    {
        .level_idx = 17,
        .max_width = 16384,
        .max_height = 8704,
        .max_sample_rate = 2189721600,
    },
    {
        .level_idx = 18,
        .max_width = 16384,
        .max_height = 8704,
        .max_sample_rate = 4379443200,
    },
    {
        .level_idx = 19,
        .max_width = 16384,
        .max_height = 8704,
        .max_sample_rate = 4706009088,
    },
};

// Computes the "level" of the bitstream based on resolution and framerate.
// In the AV1 specifications, Annex A section A.3 provides a table for computing
// the appropriate "level" based on the samples (pixels) per second and
// resolution.
// Returns -1 when the given resolution and framerate are invalid.
int ComputeLevel(const gfx::Size& coded_size, uint32_t framerate) {
  const uint64_t samples_per_second = coded_size.GetArea() * framerate;

  for (auto& level_spec : kAV1LevelSpecs) {
    if (coded_size.width() <= level_spec.max_width &&
        coded_size.height() <= level_spec.max_height &&
        samples_per_second < level_spec.max_sample_rate) {
      return level_spec.level_idx;
    }
  }

  return -1;
}

scoped_refptr<AV1Picture> GetAV1Picture(
    const VaapiVideoEncoderDelegate::EncodeJob& job) {
  return base::WrapRefCounted(
      reinterpret_cast<AV1Picture*>(job.picture().get()));
}

void DownscaleSegmentMap(const uint8_t* src_seg_map,
                         uint32_t src_seg_size,
                         size_t num_segments,
                         uint8_t* dst_seg_map,
                         uint32_t dst_seg_size,
                         const gfx::Size& coded_size) {
  CHECK(std::has_single_bit(src_seg_size));
  CHECK(std::has_single_bit(dst_seg_size));
  CHECK_LT(src_seg_size, dst_seg_size);

  // We want to avoid doing a division operation for each src segment, so we
  // find the log of the segment size ratio and right shift by that instead to
  // calculate coordinates. This count leading zeros trick is just a fast way to
  // compute the log, since we know the segment size ratios of going to be
  // powers of two.
  const uint32_t log_seg_size_ratio =
      std::countl_zero(src_seg_size) - std::countl_zero(dst_seg_size);
  const uint32_t src_width =
      base::bits::AlignUp(static_cast<uint32_t>(coded_size.width()),
                          src_seg_size) /
      src_seg_size;
  const uint32_t src_height =
      base::bits::AlignUp(static_cast<uint32_t>(coded_size.height()),
                          src_seg_size) /
      src_seg_size;
  const uint32_t dst_width =
      base::bits::AlignUp(static_cast<uint32_t>(coded_size.width()),
                          dst_seg_size) /
      dst_seg_size;
  const uint32_t dst_height =
      base::bits::AlignUp(static_cast<uint32_t>(coded_size.height()),
                          dst_seg_size) /
      dst_seg_size;

  std::vector<uint8_t> freq_distribution(num_segments * dst_width * dst_height);

  // Two pass procedure:
  // First pass generates a frequency histogram of segment IDs.
  // Second pass writes most frequent src segment ID for each dst segment.
  for (uint32_t src_y = 0; src_y < src_height; src_y++) {
    size_t row_offset = (src_y >> log_seg_size_ratio) * dst_width;
    for (uint32_t src_x = 0; src_x < src_width; src_x++) {
      DCHECK_LT(*src_seg_map, num_segments);
      freq_distribution[(row_offset + (src_x >> log_seg_size_ratio)) *
                            num_segments +
                        *src_seg_map]++;
      src_seg_map++;
    }
  }
  for (uint32_t dst_y = 0; dst_y < dst_height; dst_y++) {
    size_t row_offset = dst_y * dst_width;
    for (uint32_t dst_x = 0; dst_x < dst_width; dst_x++) {
      int most_freq = -1;
      int freq = -1;
      const size_t segment_offset = (row_offset + dst_x) * num_segments;
      for (size_t i = 0; i < num_segments; i++) {
        if (freq_distribution[segment_offset + i] > freq) {
          freq = freq_distribution[segment_offset + i];
          most_freq = i;
        }
      }
      *dst_seg_map = most_freq;
      dst_seg_map++;
    }
  }
}

AV1BitstreamBuilder::SequenceHeader FillAV1BuilderSequenceHeader(
    const gfx::Size& visible_size,
    int level_idx) {
  AV1BitstreamBuilder::SequenceHeader sequence_header;

  // The only known hardware that supports AV1 encoding only uses profile 0.
  sequence_header.profile = 0;
  sequence_header.level = level_idx;
  sequence_header.tier = 0;
  sequence_header.frame_width_bits_minus_1 = 15;
  sequence_header.frame_height_bits_minus_1 = 15;
  sequence_header.width = visible_size.width();
  sequence_header.height = visible_size.height();

  sequence_header.use_128x128_superblock = false;
  sequence_header.enable_filter_intra = false;
  sequence_header.enable_intra_edge_filter = false;
  sequence_header.enable_interintra_compound = false;
  sequence_header.enable_masked_compound = false;
  sequence_header.enable_warped_motion = false;
  sequence_header.enable_dual_filter = false;
  sequence_header.enable_order_hint = true;
  sequence_header.enable_jnt_comp = false;
  sequence_header.enable_ref_frame_mvs = false;
  sequence_header.order_hint_bits_minus_1 = 7;
  sequence_header.enable_superres = false;
  sequence_header.enable_cdef = true;
  sequence_header.enable_restoration = false;

  return sequence_header;
}

AV1BitstreamBuilder::FrameHeader FillAV1BuilderFrameHeader(
    const VAEncPictureParameterBufferAV1& pic_param,
    const AV1VaapiVideoEncoderDelegate::EncodeParams& current_params) {
  AV1BitstreamBuilder::FrameHeader pic_hdr;
  libgav1::FrameType frame_type =
      static_cast<libgav1::FrameType>(pic_param.picture_flags.bits.frame_type);
  pic_hdr.frame_type = frame_type;
  pic_hdr.error_resilient_mode =
      pic_param.picture_flags.bits.error_resilient_mode;
  pic_hdr.disable_cdf_update = pic_param.picture_flags.bits.disable_cdf_update;
  pic_hdr.disable_frame_end_update_cdf =
      pic_param.picture_flags.bits.disable_frame_end_update_cdf;
  pic_hdr.base_qindex = pic_param.base_qindex;
  pic_hdr.order_hint = pic_param.order_hint;
  pic_hdr.filter_level[0] = pic_param.filter_level[0];
  pic_hdr.filter_level[1] = pic_param.filter_level[1];
  pic_hdr.filter_level_u = pic_param.filter_level_u;
  pic_hdr.filter_level_v = pic_param.filter_level_v;
  pic_hdr.sharpness_level = pic_param.loop_filter_flags.bits.sharpness_level;
  // Disable loop filter delta.
  pic_hdr.loop_filter_delta_enabled = false;
  pic_hdr.primary_ref_frame = pic_param.primary_ref_frame;
  // Set all reference frame indices to 0
  for (uint8_t& ref_idx : pic_hdr.ref_frame_idx) {
    ref_idx = 0;
  }
  // Refresh frame flags for last frame.
  pic_hdr.refresh_frame_flags = 1 << (libgav1::kReferenceFrameLast - 1);
  // Set order hint for each reference frame.
  pic_hdr.ref_order_hint[0] = pic_param.order_hint - 1;
  // Since we only use the last frame as the reference, these should
  // always be 0.
  for (int i = 1; i < libgav1::kNumReferenceFrameTypes; i++) {
    pic_hdr.ref_order_hint[i] = 0;
  }

  for (size_t i = 0; i < ARRAY_SIZE(current_params.cdef_y_pri_strength); i++) {
    pic_hdr.cdef_y_pri_strength[i] = current_params.cdef_y_pri_strength[i];
    pic_hdr.cdef_y_sec_strength[i] = current_params.cdef_y_sec_strength[i];
    pic_hdr.cdef_uv_pri_strength[i] = current_params.cdef_uv_pri_strength[i];
    pic_hdr.cdef_uv_sec_strength[i] = current_params.cdef_uv_sec_strength[i];
  }
  pic_hdr.reduced_tx_set = pic_param.picture_flags.bits.reduced_tx_set;
  pic_hdr.segmentation_enabled =
      pic_param.segments.seg_flags.bits.segmentation_enabled;
  if (pic_hdr.segmentation_enabled) {
    pic_hdr.segment_number = pic_param.segments.segment_number;
    pic_hdr.segmentation_update_map =
        pic_param.segments.seg_flags.bits.segmentation_update_map;
    pic_hdr.segmentation_temporal_update =
        pic_param.segments.seg_flags.bits.segmentation_temporal_update;
    pic_hdr.segmentation_update_data = true;
    for (uint32_t i = 0; i < pic_hdr.segment_number; i++) {
      pic_hdr.feature_data[i][0] = pic_param.segments.feature_data[i][0];
      pic_hdr.feature_mask[i] = pic_param.segments.feature_mask[i];
    }
  }
  return pic_hdr;
}

}  // namespace

AV1VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : intra_period(kKFPeriod), framerate(0), min_qp(kMinQP), max_qp(kMaxQP) {}

AV1VaapiVideoEncoderDelegate::AV1VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb) {}

bool AV1VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  if (config.output_profile != VideoCodecProfile::AV1PROFILE_PROFILE_MAIN) {
    LOG(ERROR) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    LOG(ERROR) << "Input visible size cannot be empty";
    return false;
  }

  visible_size_ = config.input_visible_size;
  coded_size_ =
      gfx::Size(base::bits::AlignUpDeprecatedDoNotUse(
                    visible_size_.width(), kAV1AlignmentSize.width()),
                base::bits::AlignUpDeprecatedDoNotUse(
                    visible_size_.height(), kAV1AlignmentSize.height()));

  current_params_.framerate = config.framerate;
  current_params_.drop_frame_thresh = config.drop_frame_thresh_percentage;
  current_params_.bitrate_allocation.SetBitrate(0, 0,
                                                config.bitrate.target_bps());

  current_params_.is_screen =
      config.content_type ==
      VideoEncodeAccelerator::Config::ContentType::kDisplay;

  level_idx_ = ComputeLevel(coded_size_, current_params_.framerate);
  if (level_idx_ < 0) {
    LOG(ERROR) << "Could not compute level index";
    return false;
  }

  frame_num_ = current_params_.intra_period;

  if (!vaapi_wrapper_->GetMinAV1SegmentSize(AV1PROFILE_PROFILE_MAIN,
                                            seg_size_)) {
    LOG(ERROR) << "Could not get minimum segment size";
    return false;
  }

  uint32_t seg_map_width =
      base::bits::AlignUp(static_cast<uint32_t>(coded_size_.width()),
                          seg_size_) /
      seg_size_;
  uint32_t seg_map_height =
      base::bits::AlignUp(static_cast<uint32_t>(coded_size_.height()),
                          seg_size_) /
      seg_size_;
  segmentation_map_.resize(seg_map_width * seg_map_height);

  return UpdateRates(current_params_.bitrate_allocation,
                     current_params_.framerate);
}

AV1VaapiVideoEncoderDelegate::~AV1VaapiVideoEncoderDelegate() = default;

bool AV1VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  // TODO(b/267521747): Implement rate control

  current_params_.bitrate_allocation = bitrate_allocation;
  current_params_.framerate = framerate;

  aom::AV1RateControlRtcConfig rc_config;
  rc_config.width = coded_size_.width();
  rc_config.height = coded_size_.height();
  // third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc
  rc_config.max_quantizer = QindexToQuantizer(current_params_.max_qp);
  rc_config.min_quantizer = QindexToQuantizer(current_params_.min_qp);
  rc_config.target_bandwidth =
      current_params_.bitrate_allocation.GetSumBps() / 1000;
  rc_config.buf_initial_sz = 600;
  rc_config.buf_optimal_sz = 600;
  rc_config.buf_sz = 1000;
  rc_config.undershoot_pct = 50;
  rc_config.overshoot_pct = 50;
  rc_config.max_intra_bitrate_pct = 300;
  rc_config.max_inter_bitrate_pct = 0;
  rc_config.frame_drop_thresh =
      base::strict_cast<int>(current_params_.drop_frame_thresh);
  rc_config.framerate = current_params_.framerate;
  rc_config.layer_target_bitrate[0] =
      current_params_.bitrate_allocation.GetSumBps() / 1000;
  rc_config.ts_rate_decimator[0] = 1;
  rc_config.aq_mode = 3;
  rc_config.ss_number_layers = 1;
  rc_config.ts_number_layers = 1;
  rc_config.min_quantizers[0] = QindexToQuantizer(current_params_.min_qp);
  rc_config.max_quantizers[0] = QindexToQuantizer(current_params_.max_qp);
  rc_config.scaling_factor_num[0] = 1;
  rc_config.scaling_factor_den[0] = 1;
  rc_config.is_screen = current_params_.is_screen;

  if (!rate_ctrl_) {
    rate_ctrl_ = aom::AV1RateControlRTC::Create(rc_config);
    return !!rate_ctrl_;
  }

  rate_ctrl_->UpdateRateControl(rc_config);
  return true;
}

gfx::Size AV1VaapiVideoEncoderDelegate::GetCodedSize() const {
  return coded_size_;
}

size_t AV1VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  return libgav1::kNumReferenceFrameTypes;
}

std::vector<gfx::Size> AV1VaapiVideoEncoderDelegate::GetSVCLayerResolutions() {
  return {visible_size_};
}

BitstreamBufferMetadata AV1VaapiVideoEncoderDelegate::GetMetadata(
    const EncodeJob& encode_job,
    size_t payload_size) {
  CHECK(!encode_job.IsFrameDropped());
  CHECK_NE(payload_size, 0u);
  BitstreamBufferMetadata metadata(
      payload_size, encode_job.IsKeyframeRequested(), encode_job.timestamp());
  CHECK(metadata.end_of_picture());
  auto picture = GetAV1Picture(encode_job);
  // Revisit populating metadata.av1 if we need SVC.
  metadata.qp =
      base::strict_cast<int32_t>(picture->frame_header.quantizer.base_index);
  return metadata;
}

// We produce a bitstream with the following OBUs in order:
// 1. Temporal Delimiter OBU (section 5.6) to signal new frame.
// 2. If we're transmitting keyframe, a sequence header OBU (section 5.5).
// 3. Frame OBU (section 5.10), which consists of a FrameHeader (5.9) and
//    compressed data.
VaapiVideoEncoderDelegate::PrepareEncodeJobResult
AV1VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob& encode_job) {
  if (frame_num_ == current_params_.intra_period) {
    encode_job.ProduceKeyframe();
  }

  aom::AV1FrameParamsRTC frame_params{
      .frame_type =
          encode_job.IsKeyframeRequested() ? aom::kKeyFrame : aom::kInterFrame,
      .spatial_layer_id = 0,
      .temporal_layer_id = 0,
  };
  if (rate_ctrl_->ComputeQP(frame_params) == aom::FrameDropDecision::kDrop) {
    CHECK(!encode_job.IsKeyframeRequested());
    DVLOGF(3) << "Drop frame";
    return PrepareEncodeJobResult::kDrop;
  }

  size_t frame_header_obu_offset = 0;
  if (!SubmitTemporalDelimiter(frame_header_obu_offset)) {
    LOG(ERROR) << "Failed to submit temporal delimiter";
    return PrepareEncodeJobResult::kFail;
  }

  if (encode_job.IsKeyframeRequested()) {
    frame_num_ = 0;
    size_t sequence_header_obu_size = 0;
    if (!SubmitSequenceHeader(sequence_header_obu_size)) {
      return PrepareEncodeJobResult::kFail;
    }
    frame_header_obu_offset += sequence_header_obu_size;
  }

  // TODO(b/267521747): Rate control buffers go here
  if (!SubmitFrame(encode_job, frame_header_obu_offset)) {
    LOG(ERROR) << "Failed to submit frame";
    return PrepareEncodeJobResult::kFail;
  }

  if (!SubmitTileGroup()) {
    LOG(ERROR) << "Failed to submit file group";
    return PrepareEncodeJobResult::kFail;
  }

  frame_num_++;

  return PrepareEncodeJobResult::kSuccess;
}

void AV1VaapiVideoEncoderDelegate::BitrateControlUpdate(
    const BitstreamBufferMetadata& metadata) {
  DVLOGF(4) << "encoded chunk size=" << metadata.payload_size_bytes;
  CHECK_NE(metadata.payload_size_bytes, 0u);
  rate_ctrl_->PostEncodeUpdate(metadata.payload_size_bytes);
}

// See section 5.6 of the AV1 specification.
bool AV1VaapiVideoEncoderDelegate::SubmitTemporalDelimiter(
    size_t& temporal_delimiter_obu_size) {
  AV1BitstreamBuilder temporal_delimiter_obu;
  temporal_delimiter_obu.WriteOBUHeader(
      /*type=*/libgav1::ObuType::kObuTemporalDelimiter,
      /*extension_flag=*/false,
      /*has_size=*/true);
  temporal_delimiter_obu.WriteValueInLeb128(0);

  std::vector<uint8_t> temporal_delimiter_obu_data =
      std::move(temporal_delimiter_obu).Flush();
  temporal_delimiter_obu_size = temporal_delimiter_obu_data.size();
  return SubmitPackedData(temporal_delimiter_obu_data);
}

bool AV1VaapiVideoEncoderDelegate::SubmitSequenceHeader(
    size_t& sequence_header_obu_size) {
  sequence_header_ = FillAV1BuilderSequenceHeader(visible_size_, level_idx_);
  if (!SubmitSequenceParam()) {
    LOG(ERROR) << "Failed to submit sequence header";
    return false;
  }
  if (!SubmitSequenceHeaderOBU(sequence_header_obu_size)) {
    LOG(ERROR) << "Failed to submit packed sequence header";
    return false;
  }

  return true;
}

// TODO(b:274756117): Consider tuning these parameters.
bool AV1VaapiVideoEncoderDelegate::SubmitSequenceParam() {
  VAEncSequenceParameterBufferAV1 seq_param;
  memset(&seq_param, 0, sizeof(VAEncSequenceParameterBufferAV1));

  // The only known hardware that supports AV1 encoding only uses profile 0.
  seq_param.seq_profile = sequence_header_.profile;
  seq_param.seq_level_idx = sequence_header_.level;
  seq_param.seq_tier = sequence_header_.tier;
#if VA_CHECK_VERSION(1, 16, 0)
  seq_param.hierarchical_flag = 0;
#endif

  // Period between keyframes.
  seq_param.intra_period = current_params_.intra_period;
  // Period between an I or P frame and the next I or P frame. B frames aren't
  // enabled by default, so this parameter is generally 1.
  seq_param.ip_period = 1;

  seq_param.bits_per_second = current_params_.bitrate_allocation.GetSumBps();

  seq_param.order_hint_bits_minus_1 = sequence_header_.order_hint_bits_minus_1;

  seq_param.seq_fields.bits.still_picture = 0;
  seq_param.seq_fields.bits.use_128x128_superblock =
      sequence_header_.use_128x128_superblock;
  seq_param.seq_fields.bits.enable_filter_intra =
      sequence_header_.enable_filter_intra;
  seq_param.seq_fields.bits.enable_intra_edge_filter =
      sequence_header_.enable_intra_edge_filter;
  seq_param.seq_fields.bits.enable_interintra_compound =
      sequence_header_.enable_interintra_compound;
  seq_param.seq_fields.bits.enable_masked_compound =
      sequence_header_.enable_masked_compound;
  seq_param.seq_fields.bits.enable_warped_motion =
      sequence_header_.enable_warped_motion;
  seq_param.seq_fields.bits.enable_dual_filter =
      sequence_header_.enable_dual_filter;
  seq_param.seq_fields.bits.enable_order_hint =
      sequence_header_.enable_order_hint;
  seq_param.seq_fields.bits.enable_jnt_comp = sequence_header_.enable_jnt_comp;
  seq_param.seq_fields.bits.enable_ref_frame_mvs =
      sequence_header_.enable_ref_frame_mvs;
  seq_param.seq_fields.bits.enable_superres = sequence_header_.enable_superres;
  seq_param.seq_fields.bits.enable_cdef = sequence_header_.enable_cdef;
  seq_param.seq_fields.bits.enable_restoration =
      sequence_header_.enable_restoration;
#if VA_CHECK_VERSION(1, 15, 0)
  seq_param.seq_fields.bits.bit_depth_minus8 = 0;
  seq_param.seq_fields.bits.subsampling_x = 1;
  seq_param.seq_fields.bits.subsampling_y = 1;
#endif

  return vaapi_wrapper_->SubmitBuffer(VAEncSequenceParameterBufferType,
                                      sizeof(VAEncSequenceParameterBufferAV1),
                                      &seq_param);
}

bool AV1VaapiVideoEncoderDelegate::SubmitSequenceHeaderOBU(
    size_t& sequence_header_obu_size) {
  AV1BitstreamBuilder sequence_header_obu;

  sequence_header_obu.WriteOBUHeader(
      /*type=*/libgav1::ObuType::kObuSequenceHeader,
      /*extension_flag=*/false,
      /*has_size=*/true);

  AV1BitstreamBuilder obu_data =
      AV1BitstreamBuilder::BuildSequenceHeaderOBU(sequence_header_);
  sequence_header_obu.WriteValueInLeb128(obu_data.OutstandingBits() / 8);
  sequence_header_obu.AppendBitstreamBuffer(std::move(obu_data));

  std::vector<uint8_t> sequence_header_obu_data =
      std::move(sequence_header_obu).Flush();

  sequence_header_obu_size = sequence_header_obu_data.size();
  return SubmitPackedData(sequence_header_obu_data);
}

bool AV1VaapiVideoEncoderDelegate::SubmitFrame(const EncodeJob& job,
                                               size_t frame_header_obu_offset) {
  VAEncPictureParameterBufferAV1 pic_param{};
  VAEncSegMapBufferAV1 segment_map_param{};
  scoped_refptr<AV1Picture> pic = GetAV1Picture(job);

  if (!FillPictureParam(pic_param, segment_map_param, job, *pic)) {
    LOG(ERROR) << "Failed to fill PPS";
    return false;
  }

  // Set QP value to the Picture frame header to set the metadata qp value
  // later, in GetMetadata().
  pic->frame_header.quantizer.base_index = pic_param.base_qindex;

  size_t frame_header_obu_size_offset = 0;
  if (!SubmitFrameOBU(pic_param, frame_header_obu_size_offset)) {
    LOG(ERROR) << "Failed to submit packed picture header";
    return false;
  }
  pic_param.byte_offset_frame_hdr_obu_size =
      frame_header_obu_offset + frame_header_obu_size_offset;
  if (!SubmitPictureParam(pic_param)) {
    LOG(ERROR) << "Failed to submit picture header";
    return false;
  }
  if (!SubmitSegmentMap(segment_map_param)) {
    LOG(ERROR) << "Failed to submit segment map";
    return false;
  }

  last_frame_ = pic;

  return true;
}

// Fill the Picture Parameter struct.
// Sensible default values for most parameters taken from
// https://github.com/intel/libva-utils/blob/master/encode/av1encode.c
// TODO(b:274756117): Tune these parameters
bool AV1VaapiVideoEncoderDelegate::FillPictureParam(
    VAEncPictureParameterBufferAV1& pic_param,
    VAEncSegMapBufferAV1& segment_map_param,
    const EncodeJob& job,
    const AV1Picture& pic) {
  const bool is_keyframe = job.IsKeyframeRequested();

  pic_param.frame_height_minus_1 = visible_size_.height() - 1;
  pic_param.frame_width_minus_1 = visible_size_.width() - 1;

  pic_param.coded_buf = job.coded_buffer_id();
  pic_param.reconstructed_frame = reinterpret_cast<const VaapiAV1Picture*>(&pic)
                                      ->reconstruct_va_surface_id();
  for (int i = 0; i < libgav1::kNumReferenceFrameTypes; i++) {
    pic_param.reference_frames[i] = VA_INVALID_ID;
  }
  for (int i = 0; i < libgav1::kNumInterReferenceFrameTypes; i++) {
    pic_param.ref_frame_idx[i] = 0;
  }

#if VA_CHECK_VERSION(1, 16, 0)
  pic_param.hierarchical_level_plus1 = 0;
#else
  pic_param.reserved8bits0 = 0;
#endif
  pic_param.primary_ref_frame = is_keyframe ? kPrimaryReferenceNone : 0;

  pic_param.order_hint = frame_num_ & 0xFF;

  pic_param.ref_frame_ctrl_l0.value = 0;
  pic_param.ref_frame_ctrl_l1.value = 0;

  if (!is_keyframe) {
    if (!last_frame_) {
      LOG(ERROR) << "Tried to produce interframe but have no reference frame";
      return false;
    }
    // AV1 supports up to 8 reference frames, but we're only using the most
    // recent frame.
    pic_param.reference_frames[0] =
        reinterpret_cast<VaapiAV1Picture*>(last_frame_.get())
            ->reconstruct_va_surface_id();
    pic_param.ref_frame_ctrl_l0.fields.search_idx0 =
        libgav1::kReferenceFrameLast;
    pic_param.ref_frame_ctrl_l1.fields.search_idx0 =
        libgav1::kReferenceFrameIntra;
  }

  pic_param.picture_flags.bits.frame_type =
      is_keyframe ? libgav1::FrameType::kFrameKey
                  : libgav1::FrameType::kFrameInter;
  // TODO(b/275080119): Turn on error resilient mode once driver bug is fixed.
  pic_param.picture_flags.bits.error_resilient_mode = 0;
  pic_param.picture_flags.bits.disable_cdf_update = 0;
  pic_param.picture_flags.bits.use_superres = 0;
  pic_param.picture_flags.bits.allow_high_precision_mv = 0;
  pic_param.picture_flags.bits.use_ref_frame_mvs = 0;
  pic_param.picture_flags.bits.disable_frame_end_update_cdf = 0;
  pic_param.picture_flags.bits.reduced_tx_set = 1;
  pic_param.picture_flags.bits.enable_frame_obu = 1;
  pic_param.picture_flags.bits.long_term_reference = 0;
  pic_param.picture_flags.bits.disable_frame_recon = 0;
  pic_param.picture_flags.bits.allow_intrabc = 0;
  pic_param.picture_flags.bits.palette_mode_enable = 0;

  switch (seg_size_) {
    case 16:
      pic_param.seg_id_block_size = 0;
      break;
    case 32:
      pic_param.seg_id_block_size = 1;
      break;
    case 64:
      pic_param.seg_id_block_size = 2;
      break;
    case 8:
      pic_param.seg_id_block_size = 3;
      break;
    default:
      LOG(ERROR) << "Invalid segment block size: " << seg_size_;
      return false;
  }

  pic_param.num_tile_groups_minus1 = 0;

  pic_param.temporal_id = 0;

  pic_param.base_qindex = rate_ctrl_->GetQP();

  aom::AV1LoopfilterLevel loop_filter_level = rate_ctrl_->GetLoopfilterLevel();
  pic_param.filter_level[0] = loop_filter_level.filter_level[0];
  pic_param.filter_level[1] = loop_filter_level.filter_level[1];
  pic_param.filter_level_u = loop_filter_level.filter_level_u;
  pic_param.filter_level_v = loop_filter_level.filter_level_v;

  aom::AV1SegmentationData seg_data;
  constexpr uint32_t kSegmentGranularity = 4;
  rate_ctrl_->GetSegmentationData(&seg_data);
  CHECK_EQ(seg_data.segmentation_map_size,
           base::bits::AlignUp(static_cast<uint32_t>(coded_size_.width()),
                               kSegmentGranularity) /
               kSegmentGranularity *
               base::bits::AlignUp(static_cast<uint32_t>(coded_size_.height()),
                                   kSegmentGranularity) /
               kSegmentGranularity);
  pic_param.segments.seg_flags.bits.segmentation_enabled = 1;
  pic_param.segments.seg_flags.bits.segmentation_update_map = 1;
  pic_param.segments.seg_flags.bits.segmentation_temporal_update = 0;
  pic_param.segments.segment_number = seg_data.delta_q_size;
  for (uint32_t i = 0; i < seg_data.delta_q_size; i++) {
    pic_param.segments.feature_data[i][0] = seg_data.delta_q[i];
    pic_param.segments.feature_mask[i] =
        (1u << libgav1::kSegmentFeatureQuantizer);
  }
  segment_map_param.segmentMapDataSize = segmentation_map_.size();
  DownscaleSegmentMap(seg_data.segmentation_map, kSegmentGranularity,
                      seg_data.delta_q_size, segmentation_map_.data(),
                      seg_size_, coded_size_);
  segment_map_param.pSegmentMap = segmentation_map_.data();

  DVLOGF(4) << "qp=" << pic_param.base_qindex
            << " filter_level[0]=" << loop_filter_level.filter_level[0]
            << " filter_level[1]=" << loop_filter_level.filter_level[1]
            << " filter_level_u=" << loop_filter_level.filter_level_u
            << " filter_level_v=" << loop_filter_level.filter_level_v
            << (is_keyframe ? " (keyframe)" : "");

  pic_param.loop_filter_flags.bits.sharpness_level = 0;
  pic_param.loop_filter_flags.bits.mode_ref_delta_enabled = 0;
  pic_param.loop_filter_flags.bits.mode_ref_delta_update = 0;

  pic_param.superres_scale_denominator = 0;

  pic_param.interpolation_filter = 0;

  for (int i = 0; i < libgav1::kNumReferenceFrameTypes; i++) {
    pic_param.ref_deltas[i] = 0;
  }

  pic_param.mode_deltas[0] = 0;
  pic_param.mode_deltas[0] = 0;

  pic_param.y_dc_delta_q = 0;
  pic_param.u_dc_delta_q = 0;
  pic_param.u_ac_delta_q = 0;
  pic_param.v_dc_delta_q = 0;
  pic_param.v_ac_delta_q = 0;

  pic_param.min_base_qindex = current_params_.min_qp;
  pic_param.max_base_qindex = current_params_.max_qp;

  pic_param.qmatrix_flags.bits.using_qmatrix = 0;
  pic_param.qmatrix_flags.bits.qm_y = 0;
  pic_param.qmatrix_flags.bits.qm_u = 0;
  pic_param.qmatrix_flags.bits.qm_v = 0;

  pic_param.mode_control_flags.bits.delta_q_present = 0;
  pic_param.mode_control_flags.bits.delta_q_res = 0;
  pic_param.mode_control_flags.bits.delta_lf_res = 0;
  pic_param.mode_control_flags.bits.delta_lf_present = 1;
  pic_param.mode_control_flags.bits.delta_lf_multi = 1;
  pic_param.mode_control_flags.bits.tx_mode = libgav1::TxMode::kTxModeSelect;
  pic_param.mode_control_flags.bits.reference_mode = 0;
  pic_param.mode_control_flags.bits.skip_mode_present = 0;

  pic_param.tile_cols = 1;
  pic_param.tile_rows = 1;

  pic_param.width_in_sbs_minus_1[0] =
      (coded_size_.width() / kAV1AlignmentSize.width()) - 1;
  pic_param.height_in_sbs_minus_1[0] =
      (coded_size_.height() / kAV1AlignmentSize.height()) - 1;

  pic_param.context_update_tile_id = 0;

  pic_param.cdef_damping_minus_3 = 5 - 3;
  pic_param.cdef_bits = 3;
  for (size_t i = 0; i < ARRAY_SIZE(current_params_.cdef_y_pri_strength); i++) {
    pic_param.cdef_y_strengths[i] =
        current_params_.cdef_y_pri_strength[i] * kCDEFStrengthDivisor +
        current_params_.cdef_y_sec_strength[i];
    pic_param.cdef_uv_strengths[i] =
        current_params_.cdef_uv_pri_strength[i] * kCDEFStrengthDivisor +
        current_params_.cdef_uv_sec_strength[i];
  }

  pic_param.loop_restoration_flags.bits.yframe_restoration_type = 0;
  pic_param.loop_restoration_flags.bits.cbframe_restoration_type = 0;
  pic_param.loop_restoration_flags.bits.crframe_restoration_type = 0;
  pic_param.loop_restoration_flags.bits.lr_unit_shift = 0;
  pic_param.loop_restoration_flags.bits.lr_uv_shift = 0;

  memset(&pic_param.wm, 0, sizeof(pic_param.wm));

  pic_param.tile_group_obu_hdr_info.bits.obu_extension_flag = 0;
  pic_param.tile_group_obu_hdr_info.bits.obu_has_size_field = 1;
  pic_param.tile_group_obu_hdr_info.bits.temporal_id = 0;
  pic_param.tile_group_obu_hdr_info.bits.spatial_id = 0;

  pic_param.number_skip_frames = 0;
  pic_param.skip_frames_reduced_size = 0;

  return true;
}

// See section 5.9 of the AV1 Specification
// AV1 is somewhat confusing in that there is both a standalone FrameHeader OBU,
// and a "sub-OBU" FrameHeader that's part of the Frame OBU. The former appears
// to be optional, while the latter does not.
bool AV1VaapiVideoEncoderDelegate::SubmitFrameOBU(
    const VAEncPictureParameterBufferAV1& pic_param,
    size_t& frame_header_obu_size_offset) {
  AV1BitstreamBuilder frame_obu;
  frame_obu.WriteOBUHeader(/*type=*/libgav1::ObuType::kObuFrame,
                           /*extension_flag=*/false,
                           /*has_size=*/true);
  frame_header_obu_size_offset = frame_obu.OutstandingBits() / 8;

  AV1BitstreamBuilder obu_data = AV1BitstreamBuilder::BuildFrameHeaderOBU(
      sequence_header_, FillAV1BuilderFrameHeader(pic_param, current_params_));
  frame_obu.WriteValueInLeb128(obu_data.OutstandingBits() / 8, 4);
  frame_obu.AppendBitstreamBuffer(std::move(obu_data));

  return SubmitPackedData(std::move(frame_obu).Flush());
}

bool AV1VaapiVideoEncoderDelegate::SubmitPictureParam(
    const VAEncPictureParameterBufferAV1& pic_param) {
  return vaapi_wrapper_->SubmitBuffer(VAEncPictureParameterBufferType,
                                      sizeof(VAEncPictureParameterBufferAV1),
                                      &pic_param);
}

bool AV1VaapiVideoEncoderDelegate::SubmitSegmentMap(
    const VAEncSegMapBufferAV1& segment_map_param) {
  return vaapi_wrapper_->SubmitBuffer(VAEncMacroblockMapBufferType,
                                      sizeof(VAEncSegMapBufferAV1),
                                      &segment_map_param);
}

bool AV1VaapiVideoEncoderDelegate::SubmitTileGroup() {
  VAEncTileGroupBufferAV1 tile_group_buffer{};

  return vaapi_wrapper_->SubmitBuffer(VAEncSliceParameterBufferType,
                                      sizeof(VAEncTileGroupBufferAV1),
                                      &tile_group_buffer);
}

bool AV1VaapiVideoEncoderDelegate::SubmitPackedData(
    const std::vector<uint8_t>& data) {
  VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

  packed_header_param_buffer.type = VAEncPackedHeaderPicture;
  packed_header_param_buffer.bit_length = data.size() * 8;
  packed_header_param_buffer.has_emulation_bytes = 0;

  return vaapi_wrapper_->SubmitBuffers(
      {{VAEncPackedHeaderParameterBufferType,
        sizeof(VAEncPackedHeaderParameterBuffer), &packed_header_param_buffer},
       {VAEncPackedHeaderDataBufferType, data.size(), data.data()}});
}

}  // namespace media
