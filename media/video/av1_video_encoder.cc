// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/av1_video_encoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/video_encoder_info.h"
#include "third_party/libaom/source/libaom/aom/aomcx.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace media {

namespace {

// Map externally visible buffer ids [0, 1, 2] to ids used by libaom.
constexpr std::array<int, 3> kExternalToLibAomBufMap = {
    0,  // LAST
    3,  // GOLDEN
    6,  // ALTREF
};
constexpr size_t kNumberOfReferenceBuffers = kExternalToLibAomBufMap.size();

void FreeCodecCtx(aom_codec_ctx_t* codec_ctx) {
  if (codec_ctx->name) {
    // Codec has been initialized, we need to destroy it.
    auto error = aom_codec_destroy(codec_ctx);
    DCHECK_EQ(error, AOM_CODEC_OK);
  }
  delete codec_ctx;
}

// If conversion is needed for given profile and frame, returns the destination
// pixel format. If no conversion is needed returns nullopt.
std::optional<VideoPixelFormat> GetConversionFormat(VideoCodecProfile profile,
                                                    VideoPixelFormat format,
                                                    bool needs_resize) {
  switch (profile) {
    case AV1PROFILE_PROFILE_MAIN:
      if ((format != PIXEL_FORMAT_NV12 && format != PIXEL_FORMAT_I420) ||
          needs_resize) {
        return PIXEL_FORMAT_I420;
      }
      break;
    case AV1PROFILE_PROFILE_HIGH:
      if (format != PIXEL_FORMAT_I444 || needs_resize) {
        return PIXEL_FORMAT_I444;
      }
      break;
    case AV1PROFILE_PROFILE_PRO:
    default:
      NOTREACHED_IN_MIGRATION();  // Checked during Initialize().
  }

  return std::nullopt;
}

aom_img_fmt GetAomImgFormat(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_I444:
      return AOM_IMG_FMT_I444;
    case PIXEL_FORMAT_NV12:
      return AOM_IMG_FMT_NV12;
    case PIXEL_FORMAT_I420:
      return AOM_IMG_FMT_I420;
    default:
      NOTREACHED();  // Enforced by prior call to
                     // GetConversionFormat().
  }
}

// Sets up a standard 3-plane image_t from `frame`.
void SetupStandardYuvPlanes(const VideoFrame& frame, aom_image_t* aom_image) {
  DCHECK_EQ(VideoFrame::NumPlanes(frame.format()), 3u);
  auto planes = base::span(aom_image->planes);
  auto stride = base::span(aom_image->stride);
  planes[AOM_PLANE_Y] =
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kY));
  planes[AOM_PLANE_U] =
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kU));
  planes[AOM_PLANE_V] =
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kV));
  stride[AOM_PLANE_Y] = frame.stride(VideoFrame::Plane::kY);
  stride[AOM_PLANE_U] = frame.stride(VideoFrame::Plane::kU);
  stride[AOM_PLANE_V] = frame.stride(VideoFrame::Plane::kV);
}

EncoderStatus SetUpAomConfig(VideoCodecProfile profile,
                             const VideoEncoder::Options& opts,
                             aom_codec_enc_cfg_t& config,
                             aom_svc_params_t& svc_params) {
  if (opts.frame_size.width() <= 0 || opts.frame_size.height() <= 0) {
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Negative width or height values.");
  }

  if (!opts.frame_size.GetCheckedArea().IsValid()) {
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Frame is too large.");
  }

  if (opts.bit_depth.value_or(8) != 8) {
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Only 8-bit depth is supported for AV1 encoding.");
  }

  // Set up general config
  switch (profile) {
    case AV1PROFILE_PROFILE_MAIN:
      if (opts.subsampling.value_or(VideoChromaSampling::k420) !=
          VideoChromaSampling::k420) {
        return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                             "Main profile only supports 4:2:0 subsampling.");
      }
      config.g_profile = 0;
      config.g_input_bit_depth = 8;
      break;

    case AV1PROFILE_PROFILE_HIGH:
      if (opts.subsampling != VideoChromaSampling::k444) {
        return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                             "High profile only supports 4:4:4 subsampling.");
      }
      config.g_profile = 1;
      config.g_input_bit_depth = 8;
      break;

    case AV1PROFILE_PROFILE_PRO:
      // We don't build libaom with high bit depth support.
      return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedProfile,
                           "Professional profile is unsupported.");

    default:
      return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedCodec,
                           "Unsupported codec.");
  }

  config.g_pass = AOM_RC_ONE_PASS;
  // libaom encoding is performed synchronously.
  config.g_lag_in_frames = 0;
  config.rc_max_quantizer = 56;
  config.rc_min_quantizer = 10;
  // Only if latency_mode is real time, a frame might be dropped.
  config.rc_dropframe_thresh =
      opts.latency_mode == VideoEncoder::LatencyMode::Realtime
          ? GetDefaultVideoEncoderDropFrameThreshold()
          : 0;

  config.rc_undershoot_pct = 50;
  config.rc_overshoot_pct = 50;
  config.rc_buf_initial_sz = 600;
  config.rc_buf_optimal_sz = 600;
  config.rc_buf_sz = 1000;
  config.g_error_resilient = 0;

  config.g_timebase.num = 1;
  config.g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // Set the number of threads based on the image width and num of cores.
  config.g_threads = GetNumberOfThreadsForSoftwareEncoding(opts.frame_size);

  // Insert keyframes at will with a given max interval
  if (opts.keyframe_interval.has_value()) {
    config.kf_mode = AOM_KF_AUTO;
    config.kf_min_dist = 0;
    config.kf_max_dist = opts.keyframe_interval.value();
  }

  uint32_t default_bitrate = GetDefaultVideoEncodeBitrate(
      opts.frame_size, opts.framerate.value_or(30));
  config.rc_end_usage = AOM_VBR;
  // The unit of rc_target_bitrate is kilobits per second.
  config.rc_target_bitrate = default_bitrate / 1000;
  if (opts.bitrate.has_value()) {
    const auto& bitrate = opts.bitrate.value();
    switch (bitrate.mode()) {
      case Bitrate::Mode::kVariable:
        config.rc_end_usage = AOM_VBR;
        break;
      case Bitrate::Mode::kConstant:
        config.rc_end_usage = AOM_CBR;
        break;
      case Bitrate::Mode::kExternal:
        // libaom doesn't have a special rate control mode for per-frame
        // quantizer. Instead we just set CBR and set
        // AV1E_SET_QUANTIZER_ONE_PASS before each frame.
        config.rc_end_usage = AOM_CBR;
        // Let the whole AV1 quantizer range to be used.
        config.rc_max_quantizer = 63;
        config.rc_min_quantizer = 1;
        break;
    }
    if (bitrate.target_bps() != 0) {
      config.rc_target_bitrate =
          base::saturated_cast<int32_t>(bitrate.target_bps()) / 1000;
    }
  }

  config.g_w = opts.frame_size.width();
  config.g_h = opts.frame_size.height();

  // Setting up SVC parameters
  svc_params = {};
  auto framerate_factor = base::span(svc_params.framerate_factor);
  auto layer_target_bitrate = base::span(svc_params.layer_target_bitrate);
  framerate_factor[0] = 1;
  svc_params.number_spatial_layers = 1;
  svc_params.number_temporal_layers = 1;
  if (opts.scalability_mode.has_value()) {
    switch (opts.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        // Nothing to do
        break;
      case SVCScalabilityMode::kL1T2:
        framerate_factor[0] = 2;
        framerate_factor[1] = 1;
        svc_params.number_temporal_layers = 2;
        // Bitrate allocation L0: 60% L1: 40%
        layer_target_bitrate[0] = 60 * config.rc_target_bitrate / 100;
        layer_target_bitrate[1] = config.rc_target_bitrate;
        break;
      case SVCScalabilityMode::kL1T3:
        framerate_factor[0] = 4;
        framerate_factor[1] = 2;
        framerate_factor[2] = 1;
        svc_params.number_temporal_layers = 3;

        // Bitrate allocation L0: 50% L1: 20% L2: 30%
        layer_target_bitrate[0] = 50 * config.rc_target_bitrate / 100;
        layer_target_bitrate[1] = 70 * config.rc_target_bitrate / 100;
        layer_target_bitrate[2] = config.rc_target_bitrate;

        break;
      default:
        return EncoderStatus(
            EncoderStatus::Codes::kEncoderUnsupportedConfig,
            "Unsupported configuration of scalability layers.");
    }
  }

  auto scaling_factor_num = base::span(svc_params.scaling_factor_num);
  auto scaling_factor_den = base::span(svc_params.scaling_factor_den);
  auto max_quantizers = base::span(svc_params.max_quantizers);
  auto min_quantizers = base::span(svc_params.min_quantizers);
  for (int i = 0; i < svc_params.number_temporal_layers; ++i) {
    scaling_factor_num[i] = 1;
    scaling_factor_den[i] = 1;
    max_quantizers[i] = config.rc_max_quantizer;
    min_quantizers[i] = config.rc_min_quantizer;
  }

  return EncoderStatus::Codes::kOk;
}

std::string LogAomErrorMessage(aom_codec_ctx_t* context,
                               const char* message,
                               aom_codec_err_t status) {
  auto formatted_msg = base::StringPrintf("%s: %s (%s)", message,
                                          aom_codec_err_to_string(status),
                                          aom_codec_error_detail(context));
  DLOG(ERROR) << formatted_msg;
  return formatted_msg;
}

}  // namespace

Av1VideoEncoder::Av1VideoEncoder() : codec_(nullptr, FreeCodecCtx) {}

void Av1VideoEncoder::Initialize(VideoCodecProfile profile,
                                 const Options& options,
                                 EncoderInfoCB info_cb,
                                 OutputCB output_cb,
                                 EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (codec_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  // libaom is compiled with CONFIG_REALTIME_ONLY, so we can't use anything
  // but AOM_USAGE_REALTIME.
  auto error = aom_codec_enc_config_default(aom_codec_av1_cx(), &config_,
                                            AOM_USAGE_REALTIME);
  if (error != AOM_CODEC_OK) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to get default AOM config.")
            .WithData("error_code", error));
    return;
  }

  // This will fail for codecs other than AV1 and invalid option mixes.
  if (auto status = SetUpAomConfig(profile, options, config_, svc_params_);
      !status.is_ok()) {
    std::move(done_cb).Run(std::move(status));
    return;
  }

  profile_ = profile;

  // Initialize an encoder instance.
  aom_codec_unique_ptr codec(new aom_codec_ctx_t, FreeCodecCtx);
  codec->name = nullptr;
  aom_codec_flags_t flags = 0;
  error = aom_codec_enc_init(codec.get(), aom_codec_av1_cx(), &config_, flags);
  if (error != AOM_CODEC_OK) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "aom_codec_enc_init() failed.")
            .WithData("error_code", error)
            .WithData("error_message", aom_codec_err_to_string(error)));
    return;
  }
  DCHECK_NE(codec->name, nullptr);

#define CALL_AOM_CONTROL(key, value)                                       \
  do {                                                                     \
    error = aom_codec_control(codec.get(), (key), (value));                \
    if (error != AOM_CODEC_OK) {                                           \
      std::move(done_cb).Run(                                              \
          EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError, \
                        "Setting " #key " failed.")                        \
              .WithData("error_code", error)                               \
              .WithData("error_message", aom_codec_err_to_string(error))); \
      return;                                                              \
    }                                                                      \
  } while (false)

  CALL_AOM_CONTROL(AV1E_SET_ROW_MT, 1);
  CALL_AOM_CONTROL(AV1E_SET_COEFF_COST_UPD_FREQ, 3);
  CALL_AOM_CONTROL(AV1E_SET_MODE_COST_UPD_FREQ, 3);
  CALL_AOM_CONTROL(AV1E_SET_MV_COST_UPD_FREQ, 3);

  CALL_AOM_CONTROL(AV1E_SET_ENABLE_TPL_MODEL, 0);
  CALL_AOM_CONTROL(AV1E_SET_DELTAQ_MODE, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_ORDER_HINT, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_OBMC, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_WARPED_MOTION, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_REF_FRAME_MVS, 0);

  CALL_AOM_CONTROL(AV1E_SET_ENABLE_CFL_INTRA, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_SMOOTH_INTRA, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_ANGLE_DELTA, 0);
  CALL_AOM_CONTROL(AV1E_SET_ENABLE_FILTER_INTRA, 0);
  CALL_AOM_CONTROL(AV1E_SET_INTRA_DEFAULT_TX_ONLY, 1);
  CALL_AOM_CONTROL(AV1E_SET_SVC_PARAMS, &svc_params_);

  if (config_.rc_end_usage == AOM_CBR) {
    CALL_AOM_CONTROL(AV1E_SET_AQ_MODE, 3);
  }

  if (options.content_hint == ContentHint::Screen) {
    CALL_AOM_CONTROL(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
    CALL_AOM_CONTROL(AV1E_SET_ENABLE_PALETTE, 1);
  } else {
    CALL_AOM_CONTROL(AV1E_SET_ENABLE_PALETTE, 0);
  }

  // Keep in mind that AV1E_SET_TILE_[COLUMNS|ROWS] uses log2 units.
  CHECK_NE(config_.g_threads, 0u);
  int log2_threads = std::log2(config_.g_threads);
  int tile_columns_log2 = 0;
  int tile_rows_log2 = 0;
  switch (log2_threads) {
    case 4:
      // For 16 threads we split the frame into 16 tiles 4x4
      // We never really use more that 16 threads.
      tile_columns_log2 = 2;
      tile_rows_log2 = 2;
      break;
    case 3:
      // For 8-15 threads we split the frame into 8 tiles 4x2
      tile_columns_log2 = 2;
      tile_rows_log2 = 1;
      break;
    case 2:
      // For 4-7 threads we split the frame into 4 tiles 2x2
      tile_columns_log2 = 1;
      tile_rows_log2 = 1;
      break;
    default:
      // Default: horizontal tiles for the number of threads rounded down
      // to the power of 2.
      tile_columns_log2 = log2_threads;
  }
  CALL_AOM_CONTROL(AV1E_SET_TILE_COLUMNS, tile_columns_log2);
  CALL_AOM_CONTROL(AV1E_SET_TILE_ROWS, tile_rows_log2);

  // AOME_SET_CPUUSED determines tradeoff between video quality and compression
  // time. Valid range: 0..10. 0 runs the slowest, and 10 runs the fastest.
  // Values 6 to 9 are usually used for realtime applications. Here we choose
  // two sides of realtime range for our 'realtime' and 'quality' modes
  // because we don't want encoding speed to drop into single digit fps
  // even in quality mode.
  const int cpu_speed = (options.latency_mode == LatencyMode::Realtime) ? 9 : 7;
  CALL_AOM_CONTROL(AOME_SET_CPUUSED, cpu_speed);
#undef CALL_AOM_CONTROL

  options_ = options;
  originally_configured_size_ = options.frame_size;
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  codec_ = std::move(codec);

  if (info_cb) {
    VideoEncoderInfo info;
    info.implementation_name = "Av1VideoEncoder";
    info.is_hardware_accelerated = false;
    info.number_of_manual_reference_buffers = kNumberOfReferenceBuffers;
    BindCallbackToCurrentLoopIfNeeded(std::move(info_cb)).Run(info);
  }

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void Av1VideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                             const EncodeOptions& encode_options,
                             EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  if (!frame) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kInvalidInputFrame,
                      "No frame provided for encoding."));
    return;
  }

  if (frame->HasMappableGpuBuffer()) {
    frame = ConvertToMemoryMappedFrame(frame);
    if (!frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kSystemAPICallError,
                        "Convert GMB frame to MemoryMappedFrame failed."));
      return;
    }
  }

  if (!frame->IsMappable()) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kInvalidInputFrame,
                      "Frame is not mappable")
            .WithData("storage type", frame->storage_type())
            .WithData("format", frame->format()));
    return;
  }

  // Format conversion or resizing may be necessary to get the frame into the
  // form needed by libaom for encoding.
  if (auto conversion_format =
          GetConversionFormat(profile_, frame->format(),
                              /*needs_resize=*/frame->visible_rect().size() !=
                                  options_.frame_size)) {
    auto temp_frame = frame_pool_.CreateFrame(
        *conversion_format, options_.frame_size, gfx::Rect(options_.frame_size),
        options_.frame_size, frame->timestamp());
    if (!temp_frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kOutOfMemoryError,
                        "Can't allocate a temporary frame for conversion"));
      return;
    }

    // If `frame->format()` is unsupported ConvertAndScale() will fail.
    auto convert_status = frame_converter_.ConvertAndScale(*frame, *temp_frame);
    if (!convert_status.is_ok()) {
      std::move(done_cb).Run(std::move(convert_status));
      return;
    }

    frame = std::move(temp_frame);
  }

  aom_image_t* image = aom_img_wrap(
      &image_, GetAomImgFormat(frame->format()), options_.frame_size.width(),
      options_.frame_size.height(), 1,
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kY)));
  DCHECK_EQ(image, &image_);

  // Resizing should have been taken care of above.
  DCHECK_EQ(frame->visible_rect().size(), options_.frame_size);
  auto planes = base::span(image_.planes);
  auto stride = base::span(image_.stride);
  switch (profile_) {
    case AV1PROFILE_PROFILE_MAIN: {
      DCHECK(frame->format() == PIXEL_FORMAT_NV12 ||
             frame->format() == PIXEL_FORMAT_I420);
      if (frame->format() == PIXEL_FORMAT_NV12) {
        planes[AOM_PLANE_Y] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kY));
        planes[AOM_PLANE_U] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kUV));
        planes[AOM_PLANE_V] = nullptr;
        stride[AOM_PLANE_Y] = frame->stride(VideoFrame::Plane::kY);
        stride[AOM_PLANE_U] = frame->stride(VideoFrame::Plane::kUV);
        stride[AOM_PLANE_V] = 0;
      } else {
        SetupStandardYuvPlanes(*frame, &image_);
      }
      break;
    }
    case AV1PROFILE_PROFILE_HIGH:
      DCHECK_EQ(frame->format(), PIXEL_FORMAT_I444);
      SetupStandardYuvPlanes(*frame, &image_);
      break;

    case AV1PROFILE_PROFILE_PRO:
    default:
      NOTREACHED_IN_MIGRATION();  // Checked during Initialize().
  }

  bool key_frame = encode_options.key_frame;
  auto duration_us = GetFrameDuration(*frame).InMicroseconds();
  last_frame_timestamp_ = frame->timestamp();
  if (last_frame_color_space_ != frame->ColorSpace()) {
    last_frame_color_space_ = frame->ColorSpace();
    key_frame = true;
    UpdateEncoderColorSpace();
  }

  auto temporal_id_status = AssignNextTemporalId(key_frame);
  if (!temporal_id_status.has_value()) {
    std::move(done_cb).Run(std::move(temporal_id_status).error());
    return;
  }

  if (encode_options.quantizer.has_value()) {
    DCHECK_EQ(options_.bitrate->mode(), Bitrate::Mode::kExternal);
    // Convert double quantizer to an integer within codec's supported range.
    int qp = static_cast<int>(std::lround(encode_options.quantizer.value()));
    qp = std::clamp(qp, static_cast<int>(config_.rc_min_quantizer),
                    static_cast<int>(config_.rc_max_quantizer));
    aom_codec_control(codec_.get(), AV1E_SET_QUANTIZER_ONE_PASS, qp);
  }

  if (options_.manual_reference_buffer_control) {
    aom_svc_ref_frame_config_t ref_frame_config = {};
    for (size_t i = 0; i < kNumberOfReferenceBuffers; i++) {
      base::span(ref_frame_config.ref_idx)[kExternalToLibAomBufMap[i]] = i;
    }

    if (encode_options.update_buffer.has_value()) {
      uint8_t update_buffer_idx = encode_options.update_buffer.value();
      if (update_buffer_idx >= kNumberOfReferenceBuffers) {
        std::move(done_cb).Run(
            EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                          "update_buffer is out of bounds"));
        return;
      }
      base::span(ref_frame_config.refresh)[update_buffer_idx] = 1;
    }
    for (uint8_t ref : encode_options.reference_buffers) {
      if (ref >= kNumberOfReferenceBuffers) {
        std::move(done_cb).Run(
            EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                          "reference_buffer is out of bounds"));
        return;
      }
      base::span(ref_frame_config.reference)[kExternalToLibAomBufMap[ref]] = 1;
    }

    auto error = aom_codec_control(codec_.get(), AV1E_SET_SVC_REF_FRAME_CONFIG,
                                   &ref_frame_config);
    if (error != AOM_CODEC_OK) {
      auto msg = LogAomErrorMessage(codec_.get(), "AOM encoding error", error);
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode, msg));
      return;
    }
  }

  TRACE_EVENT1("media", "aom_codec_encode", "timestamp", frame->timestamp());
  // Use artificial timestamps, so the encoder will not be misled by frame's
  // fickle timestamps when doing rate control.
  auto error =
      aom_codec_encode(codec_.get(), image, artificial_timestamp_, duration_us,
                       key_frame ? AOM_EFLAG_FORCE_KF : 0);
  artificial_timestamp_ += duration_us;

  if (error != AOM_CODEC_OK) {
    auto msg = LogAomErrorMessage(codec_.get(), "AOM encoding error", error);
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode, msg));
    return;
  }
  auto output = GetEncoderOutput(std::move(temporal_id_status).value(),
                                 frame->timestamp(), frame->ColorSpace());
  if (svc_params_.number_temporal_layers > 1) {
    // If we got an unexpected key frame, temporal_svc_frame_index needs to
    // be adjusted, because the next frame should have index 1.
    if (output.key_frame) {
      temporal_svc_frame_index_ = 0;
    }
    if (!output.data.empty()) {
      temporal_svc_frame_index_++;
    }
  }

  output_cb_.Run(std::move(output), {});
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void Av1VideoEncoder::ChangeOptions(const Options& options,
                                    OutputCB output_cb,
                                    EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  if (options.frame_size.width() > originally_configured_size_.width() ||
      options.frame_size.height() > originally_configured_size_.height()) {
    auto status = EncoderStatus(
        EncoderStatus::Codes::kEncoderUnsupportedConfig,
        "libaom doesn't support dynamically increasing frame dimensions");
    std::move(done_cb).Run(std::move(status));
    return;
  }

  aom_codec_enc_cfg_t new_config = config_;
  aom_svc_params_t new_svc_params;
  if (auto status =
          SetUpAomConfig(profile_, options, new_config, new_svc_params);
      !status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  auto error = aom_codec_enc_config_set(codec_.get(), &new_config);
  if (error != AOM_CODEC_OK) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Failed to set a new AOM config")
            .WithData("error_code", error)
            .WithData("error_message", aom_codec_err_to_string(error)));
    return;
  }

  error = aom_codec_control(codec_.get(), AV1E_SET_SVC_PARAMS, &new_svc_params);
  if (error != AOM_CODEC_OK) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Setting AV1E_SET_SVC_PARAMS failed.")
            .WithData("error_code", error)
            .WithData("error_message", aom_codec_err_to_string(error)));
    return;
  }

  config_ = new_config;
  svc_params_ = new_svc_params;
  options_ = options;
  if (!output_cb.is_null())
    output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

base::TimeDelta Av1VideoEncoder::GetFrameDuration(const VideoFrame& frame) {
  // Frame has duration in metadata, use it.
  if (frame.metadata().frame_duration.has_value())
    return frame.metadata().frame_duration.value();

  // Options have framerate specified, use it.
  if (options_.framerate.has_value())
    return base::Seconds(1.0 / options_.framerate.value());

  // No real way to figure out duration, use time passed since the last frame
  // as an educated guess, but clamp it within reasonable limits.
  constexpr auto min_duration = base::Seconds(1.0 / 60.0);
  constexpr auto max_duration = base::Seconds(1.0 / 24.0);
  auto duration = frame.timestamp() - last_frame_timestamp_;
  return std::clamp(duration, min_duration, max_duration);
}

VideoEncoderOutput Av1VideoEncoder::GetEncoderOutput(
    int temporal_id,
    base::TimeDelta timestamp,
    gfx::ColorSpace color_space) const {
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t* pkt = nullptr;
  VideoEncoderOutput output;
  // We don't give timestamps to aom_codec_encode() that's why
  // pkt->data.frame.pts can't be used here.
  output.timestamp = timestamp;
  while ((pkt = aom_codec_get_cx_data(codec_.get(), &iter)) != nullptr) {
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      // The encoder is operating synchronously. There should be exactly one
      // encoded packet, or the frame is dropped.
      // SAFETY: It's libaom documented behaviour that pkt->data.frame.buf
      // has size of pkt->data.frame.sz.
      output.data = base::HeapArray<uint8_t>::CopiedFrom(
          UNSAFE_BUFFERS({reinterpret_cast<uint8_t*>(pkt->data.frame.buf),
                          pkt->data.frame.sz}));
      output.key_frame = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;
      output.temporal_id = output.key_frame ? 0 : temporal_id;
      output.color_space = color_space;
    }
  }
  return output;
}

EncoderStatus::Or<int> Av1VideoEncoder::AssignNextTemporalId(bool key_frame) {
  if (svc_params_.number_temporal_layers <= 1) {
    return 0;
  }

  int temporal_id = 0;
  if (key_frame)
    temporal_svc_frame_index_ = 0;

  switch (svc_params_.number_temporal_layers) {
    case 2: {
      const static std::array<int, 2> kTwoTemporalLayers = {0, 1};
      temporal_id = kTwoTemporalLayers[temporal_svc_frame_index_ %
                                       kTwoTemporalLayers.size()];
      break;
    }
    case 3: {
      const static std::array<int, 4> kThreeTemporalLayers = {0, 2, 1, 2};
      temporal_id = kThreeTemporalLayers[temporal_svc_frame_index_ %
                                         kThreeTemporalLayers.size()];
      break;
    }
  }

  aom_svc_layer_id_t layer_id = {};
  layer_id.temporal_layer_id = temporal_id;

  auto error =
      aom_codec_control(codec_.get(), AV1E_SET_SVC_LAYER_ID, &layer_id);
  if (error == AOM_CODEC_OK)
    aom_codec_control(codec_.get(), AV1E_SET_ERROR_RESILIENT_MODE,
                      temporal_id > 0 ? 1 : 0);
  if (error != AOM_CODEC_OK) {
    auto msg = LogAomErrorMessage(codec_.get(),
                                  "Set AV1E_SET_SVC_LAYER_ID error", error);
    return EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode, msg);
  }
  return temporal_id;
}

Av1VideoEncoder::~Av1VideoEncoder() = default;

void Av1VideoEncoder::Flush(EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  // The libaom encoder is operating synchronously and thus doesn't have to
  // flush if and only if |g_lag_in_frames| is set to 0.
  CHECK_EQ(config_.g_lag_in_frames, 0u);
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void Av1VideoEncoder::UpdateEncoderColorSpace() {
  auto aom_cs = VideoColorSpace::FromGfxColorSpace(last_frame_color_space_);
  if (aom_cs.primaries != VideoColorSpace::PrimaryID::INVALID) {
    auto status = aom_codec_control(codec_.get(), AV1E_SET_COLOR_PRIMARIES,
                                    static_cast<int>(aom_cs.primaries));
    if (status != AOM_CODEC_OK)
      LogAomErrorMessage(codec_.get(), "Failed to set color primaries", status);
  }
  if (aom_cs.transfer != VideoColorSpace::TransferID::INVALID) {
    auto status =
        aom_codec_control(codec_.get(), AV1E_SET_TRANSFER_CHARACTERISTICS,
                          static_cast<int>(aom_cs.transfer));
    if (status != AOM_CODEC_OK)
      LogAomErrorMessage(codec_.get(), "Failed to set color transfer", status);
  }
  if (aom_cs.matrix != VideoColorSpace::MatrixID::INVALID) {
    auto status = aom_codec_control(codec_.get(), AV1E_SET_MATRIX_COEFFICIENTS,
                                    static_cast<int>(aom_cs.matrix));
    if (status != AOM_CODEC_OK)
      LogAomErrorMessage(codec_.get(), "Failed to set color matrix", status);
  }

  if (last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL ||
      last_frame_color_space_.GetRangeID() ==
          gfx::ColorSpace::RangeID::LIMITED) {
    auto status = aom_codec_control(
        codec_.get(), AV1E_SET_COLOR_RANGE,
        last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL
            ? AOM_CR_FULL_RANGE
            : AOM_CR_STUDIO_RANGE);
    if (status != AOM_CODEC_OK) {
      LogAomErrorMessage(codec_.get(), "Failed to set color range", status);
    }
  }
}

}  // namespace media
