// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/av1_video_encoder.h"

#include <algorithm>
#include <cmath>

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

void FreeCodecCtx(aom_codec_ctx_t* codec_ctx) {
  if (codec_ctx->name) {
    // Codec has been initialized, we need to destroy it.
    auto error = aom_codec_destroy(codec_ctx);
    DCHECK_EQ(error, AOM_CODEC_OK);
  }
  delete codec_ctx;
}

EncoderStatus SetUpAomConfig(const VideoEncoder::Options& opts,
                             aom_codec_enc_cfg_t& config,
                             aom_svc_params_t& svc_params) {
  if (opts.frame_size.width() <= 0 || opts.frame_size.height() <= 0)
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Negative width or height values.");

  if (!opts.frame_size.GetCheckedArea().IsValid())
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Frame is too large.");

  // Set up general config
  config.g_profile = 0;  // main
  config.g_input_bit_depth = 8;
  config.g_pass = AOM_RC_ONE_PASS;
  config.g_lag_in_frames = 0;
  config.rc_max_quantizer = 56;
  config.rc_min_quantizer = 10;
  config.rc_dropframe_thresh = 0;  // Don't drop frames

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

  if (opts.bitrate.has_value()) {
    auto& bitrate = opts.bitrate.value();
    config.rc_target_bitrate =
        base::saturated_cast<int32_t>(bitrate.target_bps()) / 1000;
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
  } else {
    config.rc_end_usage = AOM_VBR;
    config.rc_target_bitrate = GetDefaultVideoEncodeBitrate(
        opts.frame_size, opts.framerate.value_or(30));
  }

  config.g_w = opts.frame_size.width();
  config.g_h = opts.frame_size.height();

  // Setting up SVC parameters
  svc_params = {};
  svc_params.framerate_factor[0] = 1;
  svc_params.number_spatial_layers = 1;
  svc_params.number_temporal_layers = 1;
  if (opts.scalability_mode.has_value()) {
    switch (opts.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        // Nothing to do
        break;
      case SVCScalabilityMode::kL1T2:
        svc_params.framerate_factor[0] = 2;
        svc_params.framerate_factor[1] = 1;
        svc_params.number_temporal_layers = 2;
        // Bitrate allocation L0: 60% L1: 40%
        svc_params.layer_target_bitrate[0] =
            60 * config.rc_target_bitrate / 100;
        svc_params.layer_target_bitrate[1] = config.rc_target_bitrate;
        break;
      case SVCScalabilityMode::kL1T3:
        svc_params.framerate_factor[0] = 4;
        svc_params.framerate_factor[1] = 2;
        svc_params.framerate_factor[2] = 1;
        svc_params.number_temporal_layers = 3;

        // Bitrate allocation L0: 50% L1: 20% L2: 30%
        svc_params.layer_target_bitrate[0] =
            50 * config.rc_target_bitrate / 100;
        svc_params.layer_target_bitrate[1] =
            70 * config.rc_target_bitrate / 100;
        svc_params.layer_target_bitrate[2] = config.rc_target_bitrate;

        break;
      default:
        return EncoderStatus(
            EncoderStatus::Codes::kEncoderUnsupportedConfig,
            "Unsupported configuration of scalability layers.");
    }
  }

  for (int i = 0; i < svc_params.number_temporal_layers; ++i) {
    svc_params.scaling_factor_num[i] = 1;
    svc_params.scaling_factor_den[i] = 1;
    svc_params.max_quantizers[i] = config.rc_max_quantizer;
    svc_params.min_quantizers[i] = config.rc_min_quantizer;
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
  profile_ = profile;
  if (profile != AV1PROFILE_PROFILE_MAIN) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedProfile)
            .WithData("profile", profile));
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

  if (auto status = SetUpAomConfig(options, config_, svc_params_);
      !status.is_ok()) {
    std::move(done_cb).Run(std::move(status));
    return;
  }

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

  if (config_.rc_end_usage == AOM_CBR)
    CALL_AOM_CONTROL(AV1E_SET_AQ_MODE, 3);

  CALL_AOM_CONTROL(AV1E_SET_TILE_COLUMNS,
                   static_cast<int>(std::log2(config_.g_threads)));

  // AOME_SET_CPUUSED determines tradeoff between video quality and compression
  // time. Valid range: 0..10. 0 runs the slowest, and 10 runs the fastest.
  // Values 6 to 9 are usually used for realtime applications. Here we choose
  // two sides of realtime range for our 'realtime' and 'quality' modes
  // because we don't want encoding speed to drop into single digit fps
  // even in quality mode.
  const int cpu_speed =
      (options.latency_mode == VideoEncoder::LatencyMode::Realtime) ? 9 : 7;
  CALL_AOM_CONTROL(AOME_SET_CPUUSED, cpu_speed);
#undef CALL_AOM_CONTROL

  options_ = options;
  originally_configured_size_ = options.frame_size;
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  codec_ = std::move(codec);

  VideoEncoderInfo info;
  info.implementation_name = "Av1VideoEncoder";
  info.is_hardware_accelerated = false;
  BindCallbackToCurrentLoopIfNeeded(std::move(info_cb)).Run(info);

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
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                      "No frame provided for encoding."));
    return;
  }

  bool supported_format = frame->format() == PIXEL_FORMAT_NV12 ||
                          frame->format() == PIXEL_FORMAT_I420 ||
                          frame->format() == PIXEL_FORMAT_XBGR ||
                          frame->format() == PIXEL_FORMAT_XRGB ||
                          frame->format() == PIXEL_FORMAT_ABGR ||
                          frame->format() == PIXEL_FORMAT_ARGB;
  if ((!frame->IsMappable() && !frame->HasGpuMemoryBuffer()) ||
      !supported_format) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                      "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("HasGpuMemoryBuffer", frame->HasGpuMemoryBuffer())
            .WithData("format", frame->format()));
    return;
  }

  if (frame->HasGpuMemoryBuffer()) {
    frame = ConvertToMemoryMappedFrame(frame);
    if (!frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Convert GMB frame to MemoryMappedFrame failed."));
      return;
    }
  }

  const bool is_yuv = IsYuvPlanar(frame->format());
  if (frame->visible_rect().size() != options_.frame_size || !is_yuv) {
    auto resized_frame = frame_pool_.CreateFrame(
        PIXEL_FORMAT_I420, options_.frame_size, gfx::Rect(options_.frame_size),
        options_.frame_size, frame->timestamp());

    if (resized_frame) {
      auto conv_status =
          ConvertAndScaleFrame(*frame, *resized_frame, resize_buf_);
      if (!conv_status.is_ok()) {
        std::move(done_cb).Run(
            EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode)
                .AddCause(std::move(conv_status)));
        return;
      }
    } else {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Can't allocate a resized frame."));
      return;
    }
    frame = std::move(resized_frame);
  }

  aom_img_fmt fmt = frame->format() == PIXEL_FORMAT_NV12 ? AOM_IMG_FMT_NV12
                                                         : AOM_IMG_FMT_I420;
  aom_image_t* image = aom_img_wrap(
      &image_, fmt, options_.frame_size.width(), options_.frame_size.height(),
      1, const_cast<uint8_t*>(frame->visible_data(VideoFrame::kYPlane)));
  DCHECK_EQ(image, &image_);

  switch (frame->format()) {
    case PIXEL_FORMAT_I420:
      image->planes[AOM_PLANE_Y] =
          const_cast<uint8_t*>(frame->visible_data(VideoFrame::kYPlane));
      image->planes[AOM_PLANE_U] =
          const_cast<uint8_t*>(frame->visible_data(VideoFrame::kUPlane));
      image->planes[AOM_PLANE_V] =
          const_cast<uint8_t*>(frame->visible_data(VideoFrame::kVPlane));
      image->stride[AOM_PLANE_Y] = frame->stride(VideoFrame::kYPlane);
      image->stride[AOM_PLANE_U] = frame->stride(VideoFrame::kUPlane);
      image->stride[AOM_PLANE_V] = frame->stride(VideoFrame::kVPlane);
      break;
    case PIXEL_FORMAT_NV12:
      image->planes[AOM_PLANE_Y] =
          const_cast<uint8_t*>(frame->visible_data(VideoFrame::kYPlane));
      image->planes[AOM_PLANE_U] =
          const_cast<uint8_t*>(frame->visible_data(VideoFrame::kUVPlane));
      image->planes[AOM_PLANE_V] = nullptr;
      image->stride[AOM_PLANE_Y] = frame->stride(VideoFrame::kYPlane);
      image->stride[AOM_PLANE_U] = frame->stride(VideoFrame::kUVPlane);
      image->stride[AOM_PLANE_V] = 0;
      break;
    default:
      NOTREACHED();
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
  DrainOutputs(std::move(temporal_id_status).value(), frame->timestamp(),
               frame->ColorSpace());
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
  if (auto status = SetUpAomConfig(options, new_config, new_svc_params);
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

void Av1VideoEncoder::DrainOutputs(int temporal_id,
                                   base::TimeDelta ts,
                                   gfx::ColorSpace color_space) {
  const aom_codec_cx_pkt_t* pkt = nullptr;
  aom_codec_iter_t iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(codec_.get(), &iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT)
      continue;

    VideoEncoderOutput result;
    result.key_frame = (pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0;
    result.timestamp = ts;
    result.color_space = color_space;
    result.size = pkt->data.frame.sz;

    if (result.key_frame) {
      // If we got an unexpected key frame, temporal_svc_frame_index needs to
      // be adjusted, because the next frame should have index 1.
      temporal_svc_frame_index_ = 1;
      result.temporal_id = 0;
    } else {
      result.temporal_id = temporal_id;
    }

    result.data = std::make_unique<uint8_t[]>(result.size);
    memcpy(result.data.get(), pkt->data.frame.buf, result.size);
    output_cb_.Run(std::move(result), {});
  }
}

EncoderStatus::Or<int> Av1VideoEncoder::AssignNextTemporalId(bool key_frame) {
  if (svc_params_.number_temporal_layers < 2)
    return 0;

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

  temporal_svc_frame_index_++;

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

  auto error = aom_codec_encode(codec_.get(), nullptr, 0, 0, 0);
  if (error != AOM_CODEC_OK) {
    auto msg = LogAomErrorMessage(codec_.get(), "AOM encoding error", error);
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode, msg));
    return;
  }

  // We don't call DrainOutputs() because Flush() is not expected to produce any
  // outputs. The encoder configured with g_lag_in_frames = 0 and all outputs
  // are drained after each Encode(). We might want to change this in the
  // future, see: crbug.com/1280404
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
      LogAomErrorMessage(codec_.get(), "Failed to set color transfer", status);
  }

  if (last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL ||
      last_frame_color_space_.GetRangeID() ==
          gfx::ColorSpace::RangeID::LIMITED) {
    auto status = aom_codec_control(
        codec_.get(), AV1E_SET_COLOR_RANGE,
        last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL
            ? AOM_CR_FULL_RANGE
            : AOM_CR_STUDIO_RANGE);
    if (status != AOM_CODEC_OK)
      LogAomErrorMessage(codec_.get(), "Failed to set color range", status);
  }
}

}  // namespace media
