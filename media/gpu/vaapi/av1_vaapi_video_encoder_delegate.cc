// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/av1_vaapi_video_encoder_delegate.h"

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
// Quantization parameters for AV1. Between 0 and 255.
constexpr int kMinQIndex = 145;
constexpr int kMaxQIndex = 205;

// From //third_party/webrtc/media/engine/webrtc_video_engine.h
// These are also quantization parameters, but these are in different units than
// above. These are used in AV1 rate control and are between 0 and 63.
constexpr int kMinQP = 10;
constexpr int kMaxQP = 56;

// This needs to be 64, not 16, because of superblocks.
// TODO: Look into whether or not we can reduce alignment to 16.
constexpr gfx::Size kAV1AlignmentSize(64, 64);
constexpr int kCDEFStrengthDivisor = 4;
constexpr int kPrimaryReferenceNone = 7;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

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

// Helper class for writing packed bitstream data.
class PackedData {
 public:
  void Write(uint64_t val, int num_bits);
  void WriteBool(bool val);
  void WriteOBUHeader(libgav1::ObuType type,
                      bool extension_flag,
                      bool has_size);
  void EncodeLeb128(uint32_t value,
                    absl::optional<int> fixed_size = absl::nullopt);
  std::vector<uint8_t> Flush();
  size_t OutstandingBits() { return total_outstanding_bits_; }

 private:
  std::vector<std::pair<uint64_t, int>> queued_writes_;
  size_t total_outstanding_bits_ = 0;
};

void PackedData::Write(uint64_t val, int num_bits) {
  queued_writes_.push_back(std::make_pair(val, num_bits));
  total_outstanding_bits_ += num_bits;
}

void PackedData::WriteBool(bool val) {
  Write(val, 1);
}

std::vector<uint8_t> PackedData::Flush() {
  std::vector<uint8_t> ret;
  uint8_t curr_byte = 0;
  int rem_bits_in_byte = 8;
  for (auto queued_write : queued_writes_) {
    uint64_t val = queued_write.first;
    int outstanding_bits = queued_write.second;
    while (outstanding_bits) {
      if (rem_bits_in_byte >= outstanding_bits) {
        curr_byte |= val << (rem_bits_in_byte - outstanding_bits);
        rem_bits_in_byte -= outstanding_bits;
        outstanding_bits = 0;
      } else {
        curr_byte |= (val >> (outstanding_bits - rem_bits_in_byte)) &
                     ((1 << rem_bits_in_byte) - 1);
        outstanding_bits -= rem_bits_in_byte;
        rem_bits_in_byte = 0;
      }
      if (!rem_bits_in_byte) {
        ret.push_back(curr_byte);
        curr_byte = 0;
        rem_bits_in_byte = 8;
      }
    }
  }

  if (rem_bits_in_byte != 8) {
    ret.push_back(curr_byte);
  }

  queued_writes_.clear();
  total_outstanding_bits_ = 0;

  return ret;
}

// See section 5.3.2 of the AV1 specification.
void PackedData::WriteOBUHeader(libgav1::ObuType type,
                                bool extension_flag,
                                bool has_size) {
  DCHECK_LE(1, type);
  DCHECK_LE(type, 8);
  WriteBool(false);  // forbidden bit
  Write(base::checked_cast<uint64_t>(type), 4);
  WriteBool(extension_flag);
  WriteBool(has_size);
  WriteBool(false);  // reserved bit
}

// Encode a variable length unsigned integer of up to 4 bytes.
// Most significant bit of each byte indicates if parsing should continue, and
// the 7 least significant bits hold the actual data. So the encoded length
// may be 5 bytes under some circumstances.
// This function also has a fixed size mode where we pass in a fixed size for
// the data and the function zero pads up to that size.
// See section 4.10.5 of the AV1 specification.
void PackedData::EncodeLeb128(uint32_t value, absl::optional<int> fixed_size) {
  for (int i = 0; i < fixed_size.value_or(5); i++) {
    uint8_t curr_byte = value & 0x7F;
    value >>= 7;
    if (value || fixed_size) {
      curr_byte |= 0x80;
      Write(curr_byte, 8);
    } else {
      Write(curr_byte, 8);
      break;
    }
  }
}

scoped_refptr<AV1Picture> GetAV1Picture(
    const VaapiVideoEncoderDelegate::EncodeJob& job) {
  return base::WrapRefCounted(
      reinterpret_cast<AV1Picture*>(job.picture().get()));
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
  coded_size_ = gfx::Size(
      base::bits::AlignUp(visible_size_.width(), kAV1AlignmentSize.width()),
      base::bits::AlignUp(visible_size_.height(), kAV1AlignmentSize.height()));

  current_params_.framerate = config.initial_framerate.value_or(
      VideoEncodeAccelerator::kDefaultFramerate);
  current_params_.bitrate_allocation.SetBitrate(0, 0,
                                                config.bitrate.target_bps());

  level_idx_ = ComputeLevel(coded_size_, current_params_.framerate);
  if (level_idx_ < 0) {
    LOG(ERROR) << "Could not compute level index";
    return false;
  }

  frame_num_ = current_params_.intra_period;

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
  rc_config.max_quantizer = kMaxQP;
  rc_config.min_quantizer = kMinQP;
  rc_config.target_bandwidth =
      current_params_.bitrate_allocation.GetSumBps() / 1000;
  rc_config.buf_initial_sz = 600;
  rc_config.buf_optimal_sz = 600;
  rc_config.buf_sz = 1000;
  rc_config.undershoot_pct = 50;
  rc_config.overshoot_pct = 50;
  rc_config.max_intra_bitrate_pct = 300;
  rc_config.max_inter_bitrate_pct = 0;
  rc_config.framerate = current_params_.framerate;
  rc_config.layer_target_bitrate[0] =
      current_params_.bitrate_allocation.GetSumBps() / 1000;
  rc_config.ts_rate_decimator[0] = 1;
  rc_config.aq_mode = 0;
  rc_config.ss_number_layers = 1;
  rc_config.ts_number_layers = 1;
  rc_config.max_quantizers[0] = kMaxQP;
  rc_config.min_quantizers[0] = kMinQP;
  rc_config.scaling_factor_num[0] = 1;
  rc_config.scaling_factor_den[0] = 1;

  if (!rate_ctrl_) {
    rate_ctrl_ = AV1RateControl::Create(rc_config);
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
  auto metadata =
      VaapiVideoEncoderDelegate::GetMetadata(encode_job, payload_size);
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
bool AV1VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob& encode_job) {
  PicParamOffsets offsets;

  if (frame_num_ == current_params_.intra_period) {
    encode_job.ProduceKeyframe();
  }

  if (!SubmitTemporalDelimiter(offsets)) {
    LOG(ERROR) << "Failed to submit temporal delimiter";
    return false;
  }

  if (encode_job.IsKeyframeRequested()) {
    frame_num_ = 0;
    if (!SubmitSequenceHeader(offsets)) {
      return false;
    }
  }

  // TODO(b/267521747): Rate control buffers go here

  if (!SubmitFrame(encode_job, offsets)) {
    LOG(ERROR) << "Failed to submit frame";
    return false;
  }

  if (!SubmitTileGroup()) {
    LOG(ERROR) << "Failed to submit file group";
    return false;
  }

  frame_num_++;

  return true;
}

void AV1VaapiVideoEncoderDelegate::BitrateControlUpdate(
    const BitstreamBufferMetadata& metadata) {
  DVLOGF(4) << "encoded chunk size=" << metadata.payload_size_bytes;

  aom::AV1FrameParamsRTC frame_params;
  frame_params.frame_type =
      metadata.key_frame ? aom::kKeyFrame : aom::kInterFrame;
  frame_params.spatial_layer_id = 0;
  frame_params.temporal_layer_id = 0;
  rate_ctrl_->PostEncodeUpdate(metadata.payload_size_bytes, frame_params);
}

// See section 5.6 of the AV1 specification.
bool AV1VaapiVideoEncoderDelegate::SubmitTemporalDelimiter(
    PicParamOffsets& offsets) {
  PackedData temporal_delimiter_obu;
  temporal_delimiter_obu.WriteOBUHeader(
      /*type=*/libgav1::ObuType::kObuTemporalDelimiter,
      /*extension_flag=*/false,
      /*has_size=*/true);
  temporal_delimiter_obu.EncodeLeb128(0);

  std::vector<uint8_t> temporal_delimiter_obu_data =
      temporal_delimiter_obu.Flush();
  offsets.frame_hdr_obu_size_byte_offset = temporal_delimiter_obu_data.size();

  return SubmitPackedData(temporal_delimiter_obu_data);
}

bool AV1VaapiVideoEncoderDelegate::SubmitSequenceHeader(
    PicParamOffsets& offsets) {
  if (!SubmitSequenceParam()) {
    LOG(ERROR) << "Failed to submit sequence header";
    return false;
  }
  if (!SubmitSequenceHeaderOBU(offsets)) {
    LOG(ERROR) << "Failed to submit packed sequence header";
    return false;
  }

  return true;
}

// TODO(b:274756117): Consider tuning these parameters.
bool AV1VaapiVideoEncoderDelegate::SubmitSequenceParam() {
  memset(&seq_param_, 0, sizeof(VAEncSequenceParameterBufferAV1));

  // The only known hardware that supports AV1 encoding only uses profile 0.
  seq_param_.seq_profile = 0;
  seq_param_.seq_level_idx = level_idx_;
  seq_param_.seq_tier = 0;
#if VA_CHECK_VERSION(1, 16, 0)
  seq_param_.hierarchical_flag = 0;
#endif

  // Period between keyframes.
  seq_param_.intra_period = current_params_.intra_period;
  // Period between an I or P frame and the next I or P frame. B frames aren't
  // enabled by default, so this parameter is generally 1.
  seq_param_.ip_period = 1;

  seq_param_.bits_per_second = current_params_.bitrate_allocation.GetSumBps();

  seq_param_.order_hint_bits_minus_1 = 7;

  seq_param_.seq_fields.bits.still_picture = 0;
  seq_param_.seq_fields.bits.use_128x128_superblock = 0;
  seq_param_.seq_fields.bits.enable_filter_intra = 0;
  seq_param_.seq_fields.bits.enable_intra_edge_filter = 0;
  seq_param_.seq_fields.bits.enable_interintra_compound = 0;
  seq_param_.seq_fields.bits.enable_masked_compound = 0;
  seq_param_.seq_fields.bits.enable_warped_motion = 0;
  seq_param_.seq_fields.bits.enable_dual_filter = 0;
  seq_param_.seq_fields.bits.enable_order_hint = 1;
  seq_param_.seq_fields.bits.enable_jnt_comp = 0;
  seq_param_.seq_fields.bits.enable_ref_frame_mvs = 0;
  seq_param_.seq_fields.bits.enable_superres = 0;
  seq_param_.seq_fields.bits.enable_cdef = 1;
  seq_param_.seq_fields.bits.enable_restoration = 0;
  seq_param_.seq_fields.bits.bit_depth_minus8 = 0;
  seq_param_.seq_fields.bits.subsampling_x = 1;
  seq_param_.seq_fields.bits.subsampling_y = 1;

  return vaapi_wrapper_->SubmitBuffer(VAEncSequenceParameterBufferType,
                                      sizeof(VAEncSequenceParameterBufferAV1),
                                      &seq_param_);
}

bool AV1VaapiVideoEncoderDelegate::SubmitSequenceHeaderOBU(
    PicParamOffsets& offsets) {
  PackedData sequence_header_obu;

  sequence_header_obu.WriteOBUHeader(
      /*type=*/libgav1::ObuType::kObuSequenceHeader,
      /*extension_flag=*/false,
      /*has_size=*/true);
  std::vector<uint8_t> packed_sequence_data = PackSequenceHeader();

  sequence_header_obu.EncodeLeb128(packed_sequence_data.size());

  std::vector<uint8_t> sequence_header_obu_data = sequence_header_obu.Flush();
  sequence_header_obu_data.insert(
      sequence_header_obu_data.end(),
      std::make_move_iterator(packed_sequence_data.begin()),
      std::make_move_iterator(packed_sequence_data.end()));

  offsets.frame_hdr_obu_size_byte_offset += sequence_header_obu_data.size();

  return SubmitPackedData(sequence_header_obu_data);
}

// See AV1 specification 5.5.1
std::vector<uint8_t> AV1VaapiVideoEncoderDelegate::PackSequenceHeader() const {
  PackedData ret;

  ret.Write(seq_param_.seq_profile, 3);
  ret.WriteBool(seq_param_.seq_fields.bits.still_picture);
  ret.WriteBool(false);  // Disable reduced still picture.
  ret.WriteBool(false);  // No timing info present.
  ret.WriteBool(false);  // No initial display delay.
  ret.Write(0, 5);       // One operating point.
  ret.Write(0, 12);  // No scalability information (operating_point_idc[0] = 0)
  ret.Write(level_idx_, 5);
  if (level_idx_ > 7) {
    ret.WriteBool(seq_param_.seq_tier);
  }

  ret.Write(15, 4);                           // Width bits minus 1
  ret.Write(15, 4);                           // Height bits minus 1
  ret.Write(visible_size_.width() - 1, 16);   // Max frame width minus 1
  ret.Write(visible_size_.height() - 1, 16);  // Max frame height minus 1

  ret.WriteBool(false);  // No frame IDs present
  ret.WriteBool(seq_param_.seq_fields.bits.use_128x128_superblock);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_filter_intra);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_intra_edge_filter);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_interintra_compound);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_masked_compound);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_warped_motion);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_dual_filter);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_order_hint);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_jnt_comp);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_ref_frame_mvs);
  ret.WriteBool(true);  // Enable sequence choose screen content tools

  ret.WriteBool(false);  // Disable sequence choose integer MV
  ret.WriteBool(false);  // Disable sequence force integer MV

  ret.Write(seq_param_.order_hint_bits_minus_1, 3);

  ret.WriteBool(seq_param_.seq_fields.bits.enable_superres);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_cdef);
  ret.WriteBool(seq_param_.seq_fields.bits.enable_restoration);

  ret.WriteBool(false);  // Disable high bit depth.

  ret.WriteBool(false);  // Disable monochrome
  ret.WriteBool(false);  // No color description present
  ret.WriteBool(false);  // No color range
  ret.Write(0, 2);       // Chroma sample position = 0

  ret.WriteBool(true);  // Separate UV delta Q

  ret.WriteBool(false);  // Disable film grain

  ret.WriteBool(true);  // Trailing bit must be 1 per 5.3.4

  return ret.Flush();
}

bool AV1VaapiVideoEncoderDelegate::SubmitFrame(EncodeJob& job,
                                               PicParamOffsets& offsets) {
  VAEncPictureParameterBufferAV1 pic_param{};
  scoped_refptr<AV1Picture> pic = GetAV1Picture(job);

  if (!FillPictureParam(pic_param, job, *pic)) {
    LOG(ERROR) << "Failed to fill PPS";
    return false;
  }
  if (!SubmitFrameOBU(pic_param, offsets)) {
    LOG(ERROR) << "Failed to submit packed picture header";
    return false;
  }
  if (!SubmitPictureParam(pic_param, offsets)) {
    LOG(ERROR) << "Failed to submit picture header";
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
    const EncodeJob& job,
    const AV1Picture& pic) const {
  const bool is_keyframe = job.IsKeyframeRequested();

  pic_param.frame_height_minus_1 = visible_size_.height() - 1;
  pic_param.frame_width_minus_1 = visible_size_.width() - 1;

  pic_param.coded_buf = job.coded_buffer_id();
  pic_param.reconstructed_frame = reinterpret_cast<const VaapiAV1Picture*>(&pic)
                                      ->reconstruct_va_surface()
                                      ->id();
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
            ->reconstruct_va_surface()
            ->id();
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

  pic_param.seg_id_block_size = 0;

  pic_param.num_tile_groups_minus1 = 0;

  pic_param.temporal_id = 0;

  pic_param.filter_level[0] = 15;
  pic_param.filter_level[1] = 15;
  pic_param.filter_level_u = 8;
  pic_param.filter_level_v = 8;

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

  aom::AV1FrameParamsRTC frame_params;
  frame_params.frame_type = is_keyframe ? aom::kKeyFrame : aom::kInterFrame;
  frame_params.spatial_layer_id = 0;
  frame_params.temporal_layer_id = 0;
  // This method name is a misnomer, GetQP() actually returns the QP in QIndex
  // form.
  pic_param.base_qindex = rate_ctrl_->ComputeQP(frame_params);
  DVLOGF(4) << "qp=" << pic_param.base_qindex
            << (is_keyframe ? " (keyframe)" : "");
  pic_param.y_dc_delta_q = 0;
  pic_param.u_dc_delta_q = 0;
  pic_param.u_ac_delta_q = 0;
  pic_param.v_dc_delta_q = 0;
  pic_param.v_ac_delta_q = 0;

  pic_param.min_base_qindex = kMinQIndex;
  pic_param.max_base_qindex = kMaxQIndex;

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

  // The following are initialized in SubmitPictureParam because we need to
  // generate the rest of the bitstream to compute their value:
  // bit_offset_qindex
  // bit_offset_segmentation
  // bit_offset_loopfilter_params
  // bit_offset_cdef_params
  // size_in_bits_cdef_params
  // byte_offset_frame_hdr_obu_size
  // size_in_bits_frame_hdr_obu

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
    PicParamOffsets& offsets) {
  PackedData frame_obu;

  frame_obu.WriteOBUHeader(/*type=*/libgav1::ObuType::kObuFrame,
                           /*extension_flag=*/false,
                           /*has_size=*/true);

  std::vector<uint8_t> frame_header_data = PackFrameHeader(pic_param, offsets);

  offsets.frame_hdr_obu_size_byte_offset += frame_obu.OutstandingBits() / 8;

  frame_obu.EncodeLeb128(frame_header_data.size(), 4);

  offsets.q_idx_bit_offset += frame_obu.OutstandingBits();
  offsets.segmentation_bit_offset += frame_obu.OutstandingBits();
  offsets.loop_filter_params_bit_offset += frame_obu.OutstandingBits();
  offsets.cdef_params_bit_offset += frame_obu.OutstandingBits();
  offsets.frame_hdr_obu_size_bits += frame_obu.OutstandingBits();

  std::vector<uint8_t> frame_obu_data = frame_obu.Flush();
  frame_obu_data.insert(frame_obu_data.end(),
                        std::make_move_iterator(frame_header_data.begin()),
                        std::make_move_iterator(frame_header_data.end()));

  return SubmitPackedData(frame_obu_data);
}

// See AV1 specification 5.9.2
// Sensible default values for most parameters taken from
// https://github.com/intel/libva-utils/blob/master/encode/av1encode.c
std::vector<uint8_t> AV1VaapiVideoEncoderDelegate::PackFrameHeader(
    const VAEncPictureParameterBufferAV1& pic_param,
    PicParamOffsets& offsets) const {
  PackedData ret;
  libgav1::FrameType frame_type =
      static_cast<libgav1::FrameType>(pic_param.picture_flags.bits.frame_type);

  ret.WriteBool(false);  // Disable show existing frame

  ret.Write(frame_type, 2);  // Frame type

  ret.WriteBool(true);  // Enable show frame

  if (frame_type != libgav1::FrameType::kFrameKey) {
    ret.WriteBool(pic_param.picture_flags.bits.error_resilient_mode);
  }

  ret.Write(pic_param.picture_flags.bits.disable_cdf_update, 1);
  ret.WriteBool(false);  // Disable allow screen content tools
  ret.WriteBool(false);  // Disable frame size override flag

  ret.Write(pic_param.order_hint, 8);

  if (frame_type != libgav1::FrameType::kFrameKey) {
    // TODO(b:274756117): We may want to tune the reference frames
    if (!pic_param.picture_flags.bits.error_resilient_mode) {
      ret.Write(0, 3);  // Set primary reference frame to index 0
    }
    ret.Write(1 << (libgav1::kReferenceFrameLast - 1),
              libgav1::kNumReferenceFrameTypes);  // Refresh frame flags for
                                                  // last frame

    if (pic_param.picture_flags.bits.error_resilient_mode) {
      // Set order hint for each reference frame.
      // Since we only use the last keyframe as the reference, these should
      // always be 0.
      ret.Write(frame_num_ - 1, 8);
      for (int i = 1; i < libgav1::kNumReferenceFrameTypes; i++) {
        ret.Write(0, 8);
      }
    }

    ret.WriteBool(false);  // Disable frame reference short signaling
    for (int i = 0; i < libgav1::kNumInterReferenceFrameTypes; i++) {
      ret.Write(0, 3);  // Set all reference frame indices to 0
    }
    ret.WriteBool(false);  // Render and frame size are the same
    ret.WriteBool(false);  // No allow high precision MV
    ret.WriteBool(false);  // Filter not switchable
    ret.Write(0, 2);       // Set interpolation filter to 0
    ret.WriteBool(false);  // Motion not switchable
  } else {
    ret.WriteBool(false);  // Render and frame size are the same
  }

  ret.Write(pic_param.picture_flags.bits.disable_frame_end_update_cdf, 1);

  // Pack tile info
  ret.WriteBool(true);   // Uniform tile spacing
  ret.WriteBool(false);  // Don't increment log2 of tile cols
  ret.WriteBool(false);  // Don't increment log2 of tile rows

  // Pack quantization parameters.
  offsets.q_idx_bit_offset = ret.OutstandingBits();
  ret.Write(pic_param.base_qindex, 8);
  ret.WriteBool(false);  // No DC Y delta Q
  ret.WriteBool(false);  // U and V delta Q is same
  ret.WriteBool(false);  // No DC U delta Q
  ret.WriteBool(false);  // No AC U delta Q
  ret.WriteBool(false);  // No Qmatrix

  // Pack segmentation parameters
  offsets.segmentation_bit_offset = ret.OutstandingBits();
  ret.WriteBool(false);  // Disable segmentation
  offsets.segmentation_bit_size =
      ret.OutstandingBits() - offsets.segmentation_bit_offset;

  ret.WriteBool(false);  // No delta q present

  // Pack loop filter parameters
  offsets.loop_filter_params_bit_offset = ret.OutstandingBits();
  ret.Write(pic_param.filter_level[0], 6);
  ret.Write(pic_param.filter_level[1], 6);
  ret.Write(pic_param.filter_level_u, 6);
  ret.Write(pic_param.filter_level_v, 6);
  ret.Write(pic_param.loop_filter_flags.bits.sharpness_level,
            3);  // Set loop filter sharpness to 0
  ret.Write(pic_param.loop_filter_flags.bits.mode_ref_delta_enabled,
            1);  // Disable loop filter delta

  // Pack CDEF parameters
  offsets.cdef_params_bit_offset = ret.OutstandingBits();
  ret.Write(2, 2);  // Set CDEF damping minus 3 to 5 - 3
  ret.Write(3, 2);  // Set CDEF bits to 3
  for (size_t i = 0; i < ARRAY_SIZE(current_params_.cdef_y_pri_strength); i++) {
    ret.Write(current_params_.cdef_y_pri_strength[i], 4);
    ret.Write(current_params_.cdef_y_sec_strength[i], 2);
    ret.Write(current_params_.cdef_uv_pri_strength[i], 4);
    ret.Write(current_params_.cdef_uv_sec_strength[i], 2);
  }
  offsets.cdef_params_size_bits =
      ret.OutstandingBits() - offsets.cdef_params_bit_offset;

  ret.WriteBool(true);  // TxMode TX_MODE_SELECT

  if (frame_type != libgav1::FrameType::kFrameKey) {
    ret.WriteBool(false);  // Disable reference select
  }

  ret.WriteBool(true);  // Enabled reduced TX

  if (frame_type != libgav1::FrameType::kFrameKey) {
    for (int i = libgav1::kReferenceFrameLast;
         i <= libgav1::kReferenceFrameAlternate; i++) {
      ret.WriteBool(false);  // Set is_global[] to all zeros
    }
  }

  offsets.frame_hdr_obu_size_bits = ret.OutstandingBits();

  return ret.Flush();
}

bool AV1VaapiVideoEncoderDelegate::SubmitPictureParam(
    VAEncPictureParameterBufferAV1& pic_param,
    const PicParamOffsets& offsets) {
  // TODO(b/275711269): These should actually be 0 in CQP mode, but that results
  // in a corrupt bitstream.
  pic_param.bit_offset_qindex = offsets.q_idx_bit_offset;
  pic_param.bit_offset_segmentation = offsets.segmentation_bit_offset;
  pic_param.bit_offset_loopfilter_params =
      offsets.loop_filter_params_bit_offset;
  pic_param.bit_offset_cdef_params = offsets.cdef_params_bit_offset;
  pic_param.size_in_bits_cdef_params = offsets.cdef_params_size_bits;
  pic_param.byte_offset_frame_hdr_obu_size =
      offsets.frame_hdr_obu_size_byte_offset;
  pic_param.size_in_bits_frame_hdr_obu = offsets.frame_hdr_obu_size_bits;

  return vaapi_wrapper_->SubmitBuffer(VAEncPictureParameterBufferType,
                                      sizeof(VAEncPictureParameterBufferAV1),
                                      &pic_param);
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
