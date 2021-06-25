// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"

#include <algorithm>
#include <numeric>

#include <va/va.h>

#include "base/bits.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp9_rate_control.h"
#include "media/gpu/vaapi/vp9_svc_layers.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"

namespace media {

namespace {
// Keyframe period.
constexpr size_t kKFPeriod = 3000;

// Arbitrarily chosen bitrate window size for rate control, in ms.
constexpr int kCPBWindowSizeMs = 500;

// Quantization parameter. They are vp9 ac/dc indices and their ranges are
// 0-255. Based on WebRTC's defaults.
constexpr uint8_t kMinQP = 4;
constexpr uint8_t kMaxQP = 112;
// The upper limitation of the quantization parameter for the software rate
// controller. This is larger than |kMaxQP| because a driver might ignore the
// specified maximum quantization parameter when the driver determines the
// value, but it doesn't ignore the quantization parameter by the software rate
// controller.
constexpr uint8_t kMaxQPForSoftwareRateCtrl = 224;

// This stands for 31 as a real ac value (see rfc 8.6.1 table
// ac_qlookup[3][256]). Note: This needs to be revisited once we have 10&12 bit
// encoder support.
constexpr uint8_t kDefaultQP = 24;

// filter level may affect on quality at lower bitrates; for now,
// we set a constant value (== 10) which is what other VA-API
// implementations like libyami and gstreamer-vaapi are using.
constexpr uint8_t kDefaultLfLevel = 10;

// Convert Qindex, whose range is 0-255, to the quantizer parameter used in
// libvpx vp9 rate control, whose range is 0-63.
// Cited from //third_party/libvpx/source/libvpx/vp9/encoder/vp9_quantize.cc.
uint8_t QindexToQuantizer(uint8_t q_index) {
  constexpr uint8_t kQuantizerToQindex[] = {
      0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
      52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
      104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
      156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
      208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
  };

  for (size_t q = 0; q < base::size(kQuantizerToQindex); ++q) {
    if (kQuantizerToQindex[q] >= q_index)
      return q;
  }
  return base::size(kQuantizerToQindex) - 1;
}

// The return value is expressed as a percentage of the average. For example,
// to allocate no more than 4.5 frames worth of bitrate to a keyframe, the
// return value is 450.
uint32_t MaxSizeOfKeyframeAsPercentage(uint32_t optimal_buffer_size,
                                       uint32_t max_framerate) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scale_par.
  // Max target size = scale_par * optimal_buffer_size * targetBR[Kbps].
  // This value is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / framerate.
  // The target in % is as follows:
  const double target_size_byte_per_frame = optimal_buffer_size * 0.5;
  const uint32_t target_size_kbyte =
      target_size_byte_per_frame * max_framerate / 1000;
  const uint32_t target_size_kbyte_as_percent = target_size_kbyte * 100;

  // Don't go below 3 times the per frame bandwidth.
  constexpr uint32_t kMinIntraSizePercentage = 300u;
  return std::max(kMinIntraSizePercentage, target_size_kbyte_as_percent);
}

VideoBitrateAllocation GetDefaultVideoBitrateAllocation(
    const VideoEncodeAccelerator::Config& config) {
  DCHECK(!config.HasSpatialLayer()) << "Spatial layers are not supported.";
  VideoBitrateAllocation bitrate_allocation;
  if (!config.HasTemporalLayer()) {
    bitrate_allocation.SetBitrate(0, 0, config.initial_bitrate);
    return bitrate_allocation;
  }

  const auto& spatial_layer = config.spatial_layers[0];
  const size_t num_temporal_layers = spatial_layer.num_of_temporal_layers;
  DCHECK_GT(num_temporal_layers, 1u);
  DCHECK_LE(num_temporal_layers, 3u);
  constexpr double kTemporalLayersBitrateScaleFactors
      [][VP9SVCLayers::kMaxSupportedTemporalLayers] = {
          {0.50, 0.50, 0.00},  // For two temporal layers.
          {0.25, 0.25, 0.50},  // For three temporal layers.
      };

  const uint32_t bitrate_bps = spatial_layer.bitrate_bps;
  for (size_t i = 0; i < num_temporal_layers; ++i) {
    const double factor =
        kTemporalLayersBitrateScaleFactors[num_temporal_layers - 2][i];
    bitrate_allocation.SetBitrate(
        0 /* spatial_index */, i,
        base::checked_cast<int>(bitrate_bps * factor));
  }
  return bitrate_allocation;
}

libvpx::VP9RateControlRtcConfig CreateRateControlConfig(
    const gfx::Size encode_size,
    const VP9VaapiVideoEncoderDelegate::EncodeParams& encode_params,
    const VideoBitrateAllocation& bitrate_allocation,
    const size_t num_temporal_layers) {
  libvpx::VP9RateControlRtcConfig rc_cfg{};
  rc_cfg.rc_mode = VPX_CBR;
  rc_cfg.width = encode_size.width();
  rc_cfg.height = encode_size.height();
  rc_cfg.max_quantizer = QindexToQuantizer(encode_params.max_qp);
  rc_cfg.min_quantizer = QindexToQuantizer(encode_params.min_qp);
  // libvpx::VP9RateControlRtcConfig is kbps.
  rc_cfg.target_bandwidth = encode_params.bitrate_allocation.GetSumBps() / 1000;
  // These default values come from
  // //third_party/webrtc/modules/video_coding/codecs/vp9/vp9_impl.cc.
  rc_cfg.buf_initial_sz = 500;
  rc_cfg.buf_optimal_sz = 600;
  rc_cfg.buf_sz = 1000;
  rc_cfg.undershoot_pct = 50;
  rc_cfg.overshoot_pct = 50;
  rc_cfg.max_intra_bitrate_pct = MaxSizeOfKeyframeAsPercentage(
      rc_cfg.buf_optimal_sz, encode_params.framerate);
  rc_cfg.framerate = encode_params.framerate;

  // Spatial layers variables.
  rc_cfg.ss_number_layers = 1;
  rc_cfg.scaling_factor_num[0] = 1;
  rc_cfg.scaling_factor_den[0] = 1;
  // Fill temporal layers variables.
  rc_cfg.ts_number_layers = num_temporal_layers;
  int bitrate_sum = 0;
  for (size_t ti = 0; ti < num_temporal_layers; ti++) {
    rc_cfg.max_quantizers[ti] = rc_cfg.max_quantizer;
    rc_cfg.min_quantizers[ti] = rc_cfg.min_quantizer;
    bitrate_sum += bitrate_allocation.GetBitrateBps(0, ti);
    rc_cfg.layer_target_bitrate[ti] = bitrate_sum / 1000;
    rc_cfg.ts_rate_decimator[ti] = 1u << (num_temporal_layers - ti - 1);
  }
  return rc_cfg;
}

static scoped_refptr<base::RefCountedBytes> MakeRefCountedBytes(void* ptr,
                                                                size_t size) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(ptr), size);
}

}  // namespace

VP9VaapiVideoEncoderDelegate::EncodeParams::EncodeParams()
    : kf_period_frames(kKFPeriod),
      framerate(0),
      cpb_window_size_ms(kCPBWindowSizeMs),
      cpb_size_bits(0),
      initial_qp(kDefaultQP),
      min_qp(kMinQP),
      max_qp(kMaxQP),
      error_resilient_mode(false) {}

void VP9VaapiVideoEncoderDelegate::set_rate_ctrl_for_testing(
    std::unique_ptr<VP9RateControl> rate_ctrl) {
  rate_ctrl_ = std::move(rate_ctrl);
}

VP9VaapiVideoEncoderDelegate::VP9VaapiVideoEncoderDelegate(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingClosure error_cb)
    : VaapiVideoEncoderDelegate(std::move(vaapi_wrapper), error_cb) {}

VP9VaapiVideoEncoderDelegate::~VP9VaapiVideoEncoderDelegate() {
  // VP9VaapiVideoEncoderDelegate can be destroyed on any thread.
}

bool VP9VaapiVideoEncoderDelegate::Initialize(
    const VideoEncodeAccelerator::Config& config,
    const VaapiVideoEncoderDelegate::Config& ave_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (VideoCodecProfileToVideoCodec(config.output_profile) != kCodecVP9) {
    DVLOGF(1) << "Invalid profile: " << GetProfileName(config.output_profile);
    return false;
  }
  if (config.HasSpatialLayer()) {
    DVLOGF(1) << "Spatial layer encoding is not supported";
    return false;
  }

  if (config.input_visible_size.IsEmpty()) {
    DVLOGF(1) << "Input visible size could not be empty";
    return false;
  }

  // Even though VP9VaapiVideoEncoderDelegate might support other bitrate
  // control modes, only the kConstantQuantizationParameter is used.
  if (ave_config.bitrate_control != VaapiVideoEncoderDelegate::BitrateControl::
                                        kConstantQuantizationParameter) {
    DVLOGF(1) << "Only CQ bitrate control is supported";
    return false;
  }

  visible_size_ = config.input_visible_size;
  coded_size_ = gfx::Size(base::bits::AlignUp(visible_size_.width(), 16),
                          base::bits::AlignUp(visible_size_.height(), 16));
  current_params_ = EncodeParams();
  reference_frames_.Clear();
  frame_num_ = 0;

  auto initial_bitrate_allocation = GetDefaultVideoBitrateAllocation(config);

  size_t num_temporal_layers = 1;
  if (config.HasTemporalLayer()) {
    num_temporal_layers = config.spatial_layers[0].num_of_temporal_layers;
    if (num_temporal_layers == 1 && !config.HasSpatialLayer()) {
      VLOGF(1) << "VP9SVCLayers only supports single temporal layer with more "
                  "than one spatial layer encoding";
      return false;
    }
    if (num_temporal_layers > VP9SVCLayers::kMaxSupportedTemporalLayers) {
      VLOGF(1) << "Unsupported amount of temporal layers: "
               << num_temporal_layers;
      return false;
    }
    svc_layers_ = std::make_unique<VP9SVCLayers>(config.spatial_layers);
  }
  current_params_.max_qp = kMaxQPForSoftwareRateCtrl;

  // |rate_ctrl_| might be injected for tests.
  if (!rate_ctrl_) {
    rate_ctrl_ = VP9RateControl::Create(CreateRateControlConfig(
        visible_size_, current_params_, initial_bitrate_allocation,
        num_temporal_layers));
  }
  if (!rate_ctrl_)
    return false;

  return UpdateRates(initial_bitrate_allocation,
                     config.initial_framerate.value_or(
                         VideoEncodeAccelerator::kDefaultFramerate));
}

gfx::Size VP9VaapiVideoEncoderDelegate::GetCodedSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!coded_size_.IsEmpty());

  return coded_size_;
}

size_t VP9VaapiVideoEncoderDelegate::GetMaxNumOfRefFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return kVp9NumRefFrames;
}

bool VP9VaapiVideoEncoderDelegate::PrepareEncodeJob(EncodeJob* encode_job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (svc_layers_) {
    if (svc_layers_->UpdateEncodeJob(encode_job->IsKeyframeRequested(),
                                     current_params_.kf_period_frames)) {
      encode_job->ProduceKeyframe();
    }
  } else {
    if (encode_job->IsKeyframeRequested())
      frame_num_ = 0;

    if (frame_num_ == 0)
      encode_job->ProduceKeyframe();

    frame_num_++;
    frame_num_ %= current_params_.kf_period_frames;
  }

  scoped_refptr<VP9Picture> picture = GetPicture(encode_job);
  DCHECK(picture);

  std::array<bool, kVp9NumRefsPerFrame> ref_frames_used = {false, false, false};
  SetFrameHeader(encode_job->IsKeyframeRequested(), picture.get(),
                 &ref_frames_used);
  if (!SubmitFrameParameters(encode_job, current_params_, picture,
                             reference_frames_, ref_frames_used)) {
    LOG(ERROR) << "Failed submitting frame parameters";
    return false;
  }

  UpdateReferenceFrames(picture);
  return true;
}

BitstreamBufferMetadata VP9VaapiVideoEncoderDelegate::GetMetadata(
    EncodeJob* encode_job,
    size_t payload_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto metadata =
      VaapiVideoEncoderDelegate::GetMetadata(encode_job, payload_size);
  auto picture = GetPicture(encode_job);
  DCHECK(picture);
  metadata.vp9 = picture->metadata_for_encoding;
  return metadata;
}

void VP9VaapiVideoEncoderDelegate::BitrateControlUpdate(
    uint64_t encoded_chunk_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!rate_ctrl_) {
    DLOG(ERROR) << __func__ << "() is called when no bitrate controller exists";
    return;
  }

  DVLOGF(4) << "|encoded_chunk_size_bytes|=" << encoded_chunk_size_bytes;
  rate_ctrl_->PostEncodeUpdate(encoded_chunk_size_bytes);
}

bool VP9VaapiVideoEncoderDelegate::UpdateRates(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bitrate_allocation.GetSumBps() == 0 || framerate == 0)
    return false;

  if (current_params_.bitrate_allocation == bitrate_allocation &&
      current_params_.framerate == framerate) {
    return true;
  }
  VLOGF(2) << "New bitrate: " << bitrate_allocation.GetSumBps()
           << ", New framerate: " << framerate;

  current_params_.bitrate_allocation = bitrate_allocation;
  current_params_.framerate = framerate;

  current_params_.cpb_size_bits =
      current_params_.bitrate_allocation.GetSumBps() *
      current_params_.cpb_window_size_ms / 1000;

  if (!rate_ctrl_)
    return true;

  const size_t num_temporal_layers =
      svc_layers_ ? svc_layers_->num_temporal_layers() : 1u;
  rate_ctrl_->UpdateRateControl(CreateRateControlConfig(
      visible_size_, current_params_, bitrate_allocation, num_temporal_layers));
  return true;
}

Vp9FrameHeader VP9VaapiVideoEncoderDelegate::GetDefaultFrameHeader(
    const bool keyframe) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Vp9FrameHeader hdr{};
  DCHECK(!visible_size_.IsEmpty());
  hdr.frame_width = visible_size_.width();
  hdr.frame_height = visible_size_.height();
  hdr.render_width = visible_size_.width();
  hdr.render_height = visible_size_.height();
  hdr.quant_params.base_q_idx = kDefaultQP;
  hdr.loop_filter.level = kDefaultLfLevel;
  hdr.show_frame = true;
  hdr.frame_type =
      keyframe ? Vp9FrameHeader::KEYFRAME : Vp9FrameHeader::INTERFRAME;
  return hdr;
}

void VP9VaapiVideoEncoderDelegate::SetFrameHeader(
    bool keyframe,
    VP9Picture* picture,
    std::array<bool, kVp9NumRefsPerFrame>* ref_frames_used) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(picture);
  DCHECK(ref_frames_used);

  *picture->frame_hdr = GetDefaultFrameHeader(keyframe);
  if (svc_layers_) {
    // Reference frame settings for temporal layer stream.
    svc_layers_->FillUsedRefFramesAndMetadata(picture, ref_frames_used);
    // Enable error resilient mode so that the syntax of a frame can be decoded
    // independently of previous frames.
    picture->frame_hdr->error_resilient_mode = true;
  } else {
    // Reference frame settings for simple stream.
    if (keyframe) {
      picture->frame_hdr->refresh_frame_flags = 0xff;
      ref_frame_index_ = 0;
    } else {
      picture->frame_hdr->ref_frame_idx[0] = ref_frame_index_;
      picture->frame_hdr->ref_frame_idx[1] =
          (ref_frame_index_ - 1) & (kVp9NumRefFrames - 1);
      picture->frame_hdr->ref_frame_idx[2] =
          (ref_frame_index_ - 2) & (kVp9NumRefFrames - 1);
      ref_frame_index_ = (ref_frame_index_ + 1) % kVp9NumRefFrames;
      picture->frame_hdr->refresh_frame_flags = 1 << ref_frame_index_;
      // Use last, golden, alt frames.
      ref_frames_used->fill(true);
    }
  }

  if (!rate_ctrl_)
    return;

  libvpx::VP9FrameParamsQpRTC frame_params{};
  frame_params.frame_type =
      keyframe ? FRAME_TYPE::KEY_FRAME : FRAME_TYPE::INTER_FRAME;
  if (picture->metadata_for_encoding) {
    frame_params.temporal_layer_id =
        picture->metadata_for_encoding->temporal_idx;
  }
  rate_ctrl_->ComputeQP(frame_params);
  picture->frame_hdr->quant_params.base_q_idx = rate_ctrl_->GetQP();
  picture->frame_hdr->loop_filter.level = rate_ctrl_->GetLoopfilterLevel();
  DVLOGF(4) << "qp=" << rate_ctrl_->GetQP()
            << ", filter_level=" << rate_ctrl_->GetLoopfilterLevel();
}

void VP9VaapiVideoEncoderDelegate::UpdateReferenceFrames(
    scoped_refptr<VP9Picture> picture) {
  reference_frames_.Refresh(picture);
}

void VP9VaapiVideoEncoderDelegate::NotifyEncodedChunkSize(
    VABufferID buffer_id,
    VASurfaceID sync_surface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const uint64_t encoded_chunk_size =
      vaapi_wrapper_->GetEncodedChunkSize(buffer_id, sync_surface_id);
  if (encoded_chunk_size == 0)
    error_cb_.Run();

  BitrateControlUpdate(encoded_chunk_size);
}

scoped_refptr<VP9Picture> VP9VaapiVideoEncoderDelegate::GetPicture(
    EncodeJob* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return base::WrapRefCounted(
      reinterpret_cast<VP9Picture*>(job->picture().get()));
}

bool VP9VaapiVideoEncoderDelegate::SubmitFrameParameters(
    EncodeJob* job,
    const EncodeParams& encode_params,
    scoped_refptr<VP9Picture> pic,
    const Vp9ReferenceFrameVector& ref_frames,
    const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VAEncSequenceParameterBufferVP9 seq_param = {};

  const auto& frame_header = pic->frame_hdr;
  // TODO(crbug.com/811912): Double check whether the
  // max_frame_width or max_frame_height affects any of the memory
  // allocation and tighten these values based on that.
  constexpr gfx::Size kMaxFrameSize(4096, 4096);
  seq_param.max_frame_width = kMaxFrameSize.height();
  seq_param.max_frame_height = kMaxFrameSize.width();
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();
  seq_param.intra_period = encode_params.kf_period_frames;

  VAEncPictureParameterBufferVP9 pic_param = {};

  pic_param.frame_width_src = frame_header->frame_width;
  pic_param.frame_height_src = frame_header->frame_height;
  pic_param.frame_width_dst = frame_header->render_width;
  pic_param.frame_height_dst = frame_header->render_height;

  pic_param.reconstructed_frame = pic->AsVaapiVP9Picture()->GetVASurfaceID();
  DCHECK_NE(pic_param.reconstructed_frame, VA_INVALID_ID);

  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    auto ref_pic = ref_frames.GetFrame(i);
    pic_param.reference_frames[i] =
        ref_pic ? ref_pic->AsVaapiVP9Picture()->GetVASurfaceID()
                : VA_INVALID_ID;
  }

  pic_param.coded_buf = job->coded_buffer_id();
  DCHECK_NE(pic_param.coded_buf, VA_INVALID_ID);

  if (frame_header->IsKeyframe()) {
    pic_param.ref_flags.bits.force_kf = true;
  } else {
    // Non-key frame mode, the frame has at least 1 reference frames.
    size_t first_used_ref_frame = 3;
    for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
      if (ref_frames_used[i]) {
        first_used_ref_frame = std::min(first_used_ref_frame, i);
        pic_param.ref_flags.bits.ref_frame_ctrl_l0 |= (1 << i);
      }
    }
    CHECK_LT(first_used_ref_frame, 3u);

    pic_param.ref_flags.bits.ref_last_idx =
        ref_frames_used[0] ? frame_header->ref_frame_idx[0]
                           : frame_header->ref_frame_idx[first_used_ref_frame];
    pic_param.ref_flags.bits.ref_gf_idx =
        ref_frames_used[1] ? frame_header->ref_frame_idx[1]
                           : frame_header->ref_frame_idx[first_used_ref_frame];
    pic_param.ref_flags.bits.ref_arf_idx =
        ref_frames_used[2] ? frame_header->ref_frame_idx[2]
                           : frame_header->ref_frame_idx[first_used_ref_frame];
  }

  pic_param.pic_flags.bits.frame_type = frame_header->frame_type;
  pic_param.pic_flags.bits.show_frame = frame_header->show_frame;
  pic_param.pic_flags.bits.error_resilient_mode =
      frame_header->error_resilient_mode;
  pic_param.pic_flags.bits.intra_only = frame_header->intra_only;
  pic_param.pic_flags.bits.allow_high_precision_mv =
      frame_header->allow_high_precision_mv;
  pic_param.pic_flags.bits.mcomp_filter_type =
      frame_header->interpolation_filter;
  pic_param.pic_flags.bits.frame_parallel_decoding_mode =
      frame_header->frame_parallel_decoding_mode;
  pic_param.pic_flags.bits.reset_frame_context =
      frame_header->reset_frame_context;
  pic_param.pic_flags.bits.refresh_frame_context =
      frame_header->refresh_frame_context;
  pic_param.pic_flags.bits.frame_context_idx = frame_header->frame_context_idx;

  pic_param.refresh_frame_flags = frame_header->refresh_frame_flags;

  pic_param.luma_ac_qindex = frame_header->quant_params.base_q_idx;
  pic_param.luma_dc_qindex_delta = frame_header->quant_params.delta_q_y_dc;
  pic_param.chroma_ac_qindex_delta = frame_header->quant_params.delta_q_uv_ac;
  pic_param.chroma_dc_qindex_delta = frame_header->quant_params.delta_q_uv_dc;
  pic_param.filter_level = frame_header->loop_filter.level;
  pic_param.log2_tile_rows = frame_header->tile_rows_log2;
  pic_param.log2_tile_columns = frame_header->tile_cols_log2;

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncoderDelegate::SubmitBuffer,
                     base::Unretained(this), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddPostExecuteCallback(
      base::BindOnce(&VP9VaapiVideoEncoderDelegate::NotifyEncodedChunkSize,
                     base::Unretained(this), job->coded_buffer_id(),
                     job->input_surface()->id()));
  return true;
}

}  // namespace media
