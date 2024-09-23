// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/vpx_video_encoder.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/video_encoder_info.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace media {

namespace {

constexpr vpx_enc_frame_flags_t VP8_UPDATE_NOTHING =
    VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST;

// Frame Pattern:
// Layer Index 0: |0| |2| |4| |6| |8|
// Layer Index 1: | |1| |3| |5| |7| |
vpx_enc_frame_flags_t vp8_2layers_temporal_flags[] = {
    // Layer 0 : update and reference only last frame
    VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF |
        VP8_EFLAG_NO_UPD_ARF,

    // Layer 1: only reference last frame, no updates
    VP8_UPDATE_NOTHING | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF};

// Frame Pattern:
// Layer Index 0: |0| | | |4| | | |8| |  |  |12|
// Layer Index 1: | | |2| | | |6| | | |10|  |  |
// Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
vpx_enc_frame_flags_t vp8_3layers_temporal_flags[] = {
    // Layer 0 : update and reference only last frame
    // It only depends on layer 0
    VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF |
        VP8_EFLAG_NO_UPD_ARF,

    // Layer 2: only reference last frame, no updates
    // It only depends on layer 0
    VP8_UPDATE_NOTHING | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF,

    // Layer 1: only reference last frame, update gold frame
    // It only depends on layer 0
    VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
        VP8_EFLAG_NO_UPD_LAST,

    // Layer 2: reference last frame and gold frame, no updates
    // It depends on layer 0 and layer 1
    VP8_UPDATE_NOTHING | VP8_EFLAG_NO_REF_ARF,
};

EncoderStatus SetUpVpxConfig(const VideoEncoder::Options& opts,
                             VideoCodecProfile profile,
                             vpx_codec_enc_cfg_t* config) {
  if (opts.frame_size.width() <= 0 || opts.frame_size.height() <= 0)
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Negative width or height values.");

  if (!opts.frame_size.GetCheckedArea().IsValid())
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Frame is too large.");

  config->g_pass = VPX_RC_ONE_PASS;
  // libvpx encoding is performed synchronously.
  config->g_lag_in_frames = 0;
  config->rc_max_quantizer = 58;
  // Increase min QP to 12 for vp8 screen sharing; It reduces the encoding
  // bitrate on static content and thus helps reduce big overshoot on slide
  // change. This optimization is cited from libwebRTC.
  // third_party/webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.cc.
  // TODO(bugs.webrtc.org/15785): Set min quantizer for screen content in VP9.
  config->rc_min_quantizer = 2;
  if (profile == VP8PROFILE_ANY &&
      opts.content_hint == VideoEncoder::ContentHint::Screen) {
    config->rc_min_quantizer = 12;
  }
  config->rc_resize_allowed = 0;
  // Only if latency_mode is real time, a frame might be dropped.
  config->rc_dropframe_thresh =
      opts.latency_mode == VideoEncoder::LatencyMode::Realtime
          ? GetDefaultVideoEncoderDropFrameThreshold()
          : 0;
  config->g_timebase.num = 1;
  config->g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // Set the number of threads based on the image width and num of cores.
  config->g_threads = GetNumberOfThreadsForSoftwareEncoding(opts.frame_size);

  // Insert keyframes at will with a given max interval
  if (opts.keyframe_interval.has_value()) {
    config->kf_mode = VPX_KF_AUTO;
    config->kf_min_dist = 0;
    config->kf_max_dist = opts.keyframe_interval.value();
  }

  uint32_t default_bitrate = GetDefaultVideoEncodeBitrate(
      opts.frame_size, opts.framerate.value_or(30));
  config->rc_end_usage = VPX_VBR;
  // The unit of rc_target_bitrate is kilobits per second.
  config->rc_target_bitrate = default_bitrate / 1000;
  if (opts.bitrate.has_value()) {
    const auto& bitrate = opts.bitrate.value();
    switch (bitrate.mode()) {
      case Bitrate::Mode::kVariable:
        config->rc_end_usage = VPX_VBR;
        break;
      case Bitrate::Mode::kConstant:
        config->rc_end_usage = VPX_CBR;
        break;
      case Bitrate::Mode::kExternal:
        // libvpx doesn't have a special rate control mode for per-frame
        // quantizer. Instead we just set CBR and set
        // VP9E_SET_QUANTIZER_ONE_PASS before each frame.
        config->rc_end_usage = VPX_CBR;
        // Let the whole AV1 quantizer range to be used.
        config->rc_max_quantizer = 63;
        config->rc_min_quantizer = 0;
        break;
    }
    if (bitrate.target_bps() != 0) {
      config->rc_target_bitrate = bitrate.target_bps() / 1000;
    }
  }

  config->g_w = opts.frame_size.width();
  config->g_h = opts.frame_size.height();

  if (!opts.scalability_mode)
    return EncoderStatus::Codes::kOk;

  auto ts_layer_id = base::span(config->ts_layer_id);
  auto ts_rate_decimator = base::span(config->ts_rate_decimator);
  auto layer_target_bitrate = base::span(config->layer_target_bitrate);
  auto ts_target_bitrate = base::span(config->ts_target_bitrate);
  switch (opts.scalability_mode.value()) {
    case SVCScalabilityMode::kL1T1:
      // Nothing to do
      break;
    case SVCScalabilityMode::kL1T2:
      // Frame Pattern:
      // Layer Index 0: |0| |2| |4| |6| |8|
      // Layer Index 1: | |1| |3| |5| |7| |
      config->ts_number_layers = 2;
      config->ts_periodicity = 2;
      DCHECK_EQ(config->ts_periodicity,
                sizeof(vp8_2layers_temporal_flags) /
                    sizeof(vp8_2layers_temporal_flags[0]));
      ts_layer_id[0] = 0;
      ts_layer_id[1] = 1;
      ts_rate_decimator[0] = 2;
      ts_rate_decimator[1] = 1;
      // Bitrate allocation L0: 60% L1: 40%
      layer_target_bitrate[0] = ts_target_bitrate[0] =
          60 * config->rc_target_bitrate / 100;
      layer_target_bitrate[1] = ts_target_bitrate[1] =
          config->rc_target_bitrate;
      config->temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_0101;
      config->g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT;
      break;
    case SVCScalabilityMode::kL1T3:
      // Frame Pattern:
      // Layer Index 0: |0| | | |4| | | |8| |  |  |12|
      // Layer Index 1: | | |2| | | |6| | | |10|  |  |
      // Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
      config->ts_number_layers = 3;
      config->ts_periodicity = 4;
      DCHECK_EQ(config->ts_periodicity,
                sizeof(vp8_3layers_temporal_flags) /
                    sizeof(vp8_3layers_temporal_flags[0]));
      ts_layer_id[0] = 0;
      ts_layer_id[1] = 2;
      ts_layer_id[2] = 1;
      ts_layer_id[3] = 2;
      ts_rate_decimator[0] = 4;
      ts_rate_decimator[1] = 2;
      ts_rate_decimator[2] = 1;
      // Bitrate allocation L0: 50% L1: 20% L2: 30%
      layer_target_bitrate[0] = ts_target_bitrate[0] =
          50 * config->rc_target_bitrate / 100;
      layer_target_bitrate[1] = ts_target_bitrate[1] =
          70 * config->rc_target_bitrate / 100;
      layer_target_bitrate[2] = ts_target_bitrate[2] =
          config->rc_target_bitrate;
      config->temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_0212;
      config->g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT;
      break;
    default: {
      return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                           "Unsupported number of temporal layers.");
    }
  }

  return EncoderStatus::Codes::kOk;
}

vpx_svc_extra_cfg_t MakeSvcExtraConfig(const vpx_codec_enc_cfg_t& config) {
  vpx_svc_extra_cfg_t result = {};
  result.temporal_layering_mode = config.temporal_layering_mode;
  auto scaling_factor_num = base::span(result.scaling_factor_num);
  auto scaling_factor_den = base::span(result.scaling_factor_den);
  auto max_quantizers = base::span(result.max_quantizers);
  auto min_quantizers = base::span(result.min_quantizers);
  for (size_t i = 0; i < config.ts_number_layers; ++i) {
    scaling_factor_num[i] = 1;
    scaling_factor_den[i] = 1;
    max_quantizers[i] = config.rc_max_quantizer;
    min_quantizers[i] = config.rc_min_quantizer;
  }
  return result;
}

void FreeCodecCtx(vpx_codec_ctx_t* codec_ctx) {
  if (codec_ctx->name) {
    // Codec has been initialized, we need to destroy it.
    auto error = vpx_codec_destroy(codec_ctx);
    DCHECK_EQ(error, VPX_CODEC_OK);
  }

  delete codec_ctx;
}

std::string LogVpxErrorMessage(vpx_codec_ctx_t* context,
                               const char* message,
                               vpx_codec_err_t status) {
  auto formatted_msg = base::StringPrintf("%s: %s (%s)", message,
                                          vpx_codec_err_to_string(status),
                                          vpx_codec_error_detail(context));
  DLOG(ERROR) << formatted_msg;
  return formatted_msg;
}

// If conversion is needed for given profile and frame, returns the destination
// pixel format. If no conversion is needed returns nullopt.
std::optional<VideoPixelFormat> GetConversionFormat(VideoCodecProfile profile,
                                                    VideoPixelFormat format,
                                                    bool needs_resize) {
  switch (profile) {
    case VP8PROFILE_ANY:
    case VP9PROFILE_PROFILE0:
      if ((format != PIXEL_FORMAT_NV12 && format != PIXEL_FORMAT_I420) ||
          needs_resize) {
        return PIXEL_FORMAT_I420;
      }
      break;
    case VP9PROFILE_PROFILE1:
      if (format != PIXEL_FORMAT_I444 || needs_resize) {
        return PIXEL_FORMAT_I444;
      }
      break;
    case VP9PROFILE_PROFILE2:
      if (format != PIXEL_FORMAT_YUV420P10 || needs_resize) {
        // VideoFrameConverter doesn't support 10bit yet, so output I420 then
        // convert to I010.
        return PIXEL_FORMAT_I420;
      }
      break;
    case VP9PROFILE_PROFILE3:
      if (format != PIXEL_FORMAT_YUV444P10 || needs_resize) {
        // VideoFrameConverter doesn't support 10bit yet, so output I444 then
        // convert to I410.
        return PIXEL_FORMAT_I444;
      }
      break;
    default:
      NOTREACHED();  // Checked during Initialize().
  }
  return std::nullopt;
}

// Sets up a standard 3-plane vpx_image_t from `frame`.
void SetupStandardYuvPlanes(const VideoFrame& frame, vpx_image_t* vpx_image) {
  DCHECK_EQ(VideoFrame::NumPlanes(frame.format()), 3u);
  auto planes = base::span(vpx_image->planes);
  auto stride = base::span(vpx_image->stride);
  planes[VPX_PLANE_Y] =
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kY));
  planes[VPX_PLANE_U] =
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kU));
  planes[VPX_PLANE_V] =
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kV));
  stride[VPX_PLANE_Y] = frame.stride(VideoFrame::Plane::kY);
  stride[VPX_PLANE_U] = frame.stride(VideoFrame::Plane::kU);
  stride[VPX_PLANE_V] = frame.stride(VideoFrame::Plane::kV);
}

void I444ToI410(const VideoFrame& frame, vpx_image_t* vpx_image) {
  DCHECK_EQ(frame.format(), PIXEL_FORMAT_I444);
  auto planes = base::span(vpx_image->planes);
  auto stride = base::span(vpx_image->stride);
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame.format()); ++i) {
    libyuv::Convert8To16Plane(
        frame.visible_data(i), frame.stride(i),
        reinterpret_cast<uint16_t*>(planes[i]), stride[i] / 2, 1024,
        VideoFrame::Columns(i, frame.format(),
                            frame.visible_rect().size().width()),
        VideoFrame::Rows(i, frame.format(),
                         frame.visible_rect().size().height()));
  }
}

}  // namespace

VpxVideoEncoder::VpxVideoEncoder() : codec_(nullptr, FreeCodecCtx) {}

void VpxVideoEncoder::Initialize(VideoCodecProfile profile,
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
  bool is_vp9 = false;

  vpx_codec_iface_t* iface = nullptr;
  if (profile == VP8PROFILE_ANY) {
    iface = vpx_codec_vp8_cx();
  } else if (profile == VP9PROFILE_PROFILE0 || profile == VP9PROFILE_PROFILE1 ||
             ((profile == VP9PROFILE_PROFILE2 ||
               profile == VP9PROFILE_PROFILE3) &&
              // High bit depth encoding is not enabled on all platforms.
              (vpx_codec_get_caps(vpx_codec_vp9_cx()) &
               VPX_CODEC_CAP_HIGHBITDEPTH))) {
    is_vp9 = true;
    iface = vpx_codec_vp9_cx();
  } else {
    auto status =
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedProfile)
            .WithData("profile", profile);
    std::move(done_cb).Run(status);
    return;
  }

  if (options.bitrate.has_value() &&
      options.bitrate->mode() == Bitrate::Mode::kExternal && !is_vp9) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "VP8 doesn't support per-frame quantizer"));
    return;
  }

  auto vpx_error = vpx_codec_enc_config_default(iface, &codec_config_, 0);
  if (vpx_error != VPX_CODEC_OK) {
    auto status =
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to get default VPX config.")
            .WithData("vpx_error", vpx_error);
    std::move(done_cb).Run(status);
    return;
  }

  if (profile == VP9PROFILE_PROFILE0 || profile == VP9PROFILE_PROFILE2) {
    if (options.subsampling.value_or(VideoChromaSampling::k420) !=
        VideoChromaSampling::k420) {
      std::move(done_cb).Run(EncoderStatus(
          EncoderStatus::Codes::kEncoderUnsupportedConfig,
          "Only 4:2:0 subsampling is supported with VP9 profiles 0 and 2."));
      return;
    }
  } else if (profile == VP9PROFILE_PROFILE1 || profile == VP9PROFILE_PROFILE3) {
    // TODO(crbug.com/40144811): Support 4:2:2 subsampling.
    if (options.subsampling != VideoChromaSampling::k444) {
      std::move(done_cb).Run(EncoderStatus(
          EncoderStatus::Codes::kEncoderUnsupportedConfig,
          "Only 4:4:4 subsampling is supported with VP9 profiles 1 and 3."));
      return;
    }
  }

  if ((profile == VP9PROFILE_PROFILE0 || profile == VP9PROFILE_PROFILE1) &&
      options.bit_depth.value_or(8) != 8) {
    std::move(done_cb).Run(EncoderStatus(
        EncoderStatus::Codes::kEncoderUnsupportedConfig,
        "Only 8-bit depth is supported with VP9 profiles 0 and 1."));
    return;
  }
  if ((profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3) &&
      options.bit_depth.value_or(10) != 10) {
    std::move(done_cb).Run(EncoderStatus(
        EncoderStatus::Codes::kEncoderUnsupportedConfig,
        "Only 10-bit depth is supported with VP9 profiles 2 and 3."));
    return;
  }

  switch (profile) {
    case VP8PROFILE_ANY:
    case VP9PROFILE_PROFILE0:
      codec_config_.g_profile = 0;
      codec_config_.g_bit_depth = VPX_BITS_8;
      codec_config_.g_input_bit_depth = 8;
      break;
    case VP9PROFILE_PROFILE1:
      codec_config_.g_profile = 1;
      codec_config_.g_bit_depth = VPX_BITS_8;
      codec_config_.g_input_bit_depth = 8;
      break;
    case VP9PROFILE_PROFILE2:
      codec_config_.g_profile = 2;
      codec_config_.g_bit_depth = VPX_BITS_10;
      codec_config_.g_input_bit_depth = 10;
      break;
    case VP9PROFILE_PROFILE3:
      codec_config_.g_profile = 3;
      codec_config_.g_bit_depth = VPX_BITS_10;
      codec_config_.g_input_bit_depth = 10;
      break;
    default:
      NOTREACHED();  // Enforced via a profile check above.
  }

  auto status = SetUpVpxConfig(options, profile_, &codec_config_);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  vpx_codec_unique_ptr codec(new vpx_codec_ctx_t, FreeCodecCtx);
  codec->name = nullptr;  // We are allowed to use vpx_codec_ctx_t.name
  vpx_error = vpx_codec_enc_init(
      codec.get(), iface, &codec_config_,
      codec_config_.g_bit_depth == VPX_BITS_8 ? 0 : VPX_CODEC_USE_HIGHBITDEPTH);
  if (vpx_error != VPX_CODEC_OK) {
    auto msg = LogVpxErrorMessage(
        codec.get(), "VPX encoder initialization error", vpx_error);
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError, msg));
    return;
  }

  // For VP9 the values used for real-time encoding mode are 5, 6, 7,
  // 8, 9. Higher means faster encoding, but lower quality.
  // For VP8 typical values used for real-time encoding are -4, -6, -8,
  // -10, -12. Again larger magnitude means faster encoding but lower
  // quality.
  int cpu_used = is_vp9 ? 7 : -6;
  vpx_error = vpx_codec_control(codec.get(), VP8E_SET_CPUUSED, cpu_used);
  if (vpx_error != VPX_CODEC_OK) {
    auto msg = LogVpxErrorMessage(
        codec.get(), "VPX encoder VP8E_SET_CPUUSED error", vpx_error);
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError, msg));
    return;
  }

  if (is_vp9) {
    // Set the number of column tiles in encoding an input frame, with number of
    // tile columns (in Log2 unit) as the parameter.
    // The minimum width of a tile column is 256 pixels, the maximum is 4096.
    unsigned int tile_columns = (codec_config_.g_w + 255) / 256;
    // The valid range of VP9E_SET_TILE_COLUMNS is [0..6].
    int log2_tile_columns =
        std::min(static_cast<int>(std::log2(tile_columns)), 6);
    vpx_error = vpx_codec_control(codec.get(), VP9E_SET_TILE_COLUMNS,
                                  log2_tile_columns);
    if (vpx_error != VPX_CODEC_OK) {
      auto msg = LogVpxErrorMessage(
          codec.get(), "VPX encoder VP9E_SET_TILE_COLUMNS error", vpx_error);
      std::move(done_cb).Run(EncoderStatus(
          EncoderStatus::Codes::kEncoderInitializationError, msg));
      return;
    }

    // Turn on row level multi-threading.
    vpx_codec_control(codec.get(), VP9E_SET_ROW_MT, 1);

    if (codec_config_.ts_number_layers > 1) {
      vpx_svc_extra_cfg_t svc_conf = MakeSvcExtraConfig(codec_config_);

      // VP9 needs SVC to be turned on explicitly
      vpx_codec_control(codec.get(), VP9E_SET_SVC_PARAMETERS, &svc_conf);
      vpx_error = vpx_codec_control(codec.get(), VP9E_SET_SVC, 1);
      if (vpx_error != VPX_CODEC_OK) {
        auto msg = LogVpxErrorMessage(codec.get(),
                                      "Can't activate SVC encoding", vpx_error);
        status = EncoderStatus(
            EncoderStatus::Codes::kEncoderInitializationError, msg);
        std::move(done_cb).Run(status);
        return;
      }
    }

    // In CBR mode aq-mode=3 (cyclic refresh) is enabled for quality
    // improvement. Note: For VP8, cyclic refresh is internally done as
    // default.
    if (codec_config_.rc_end_usage == VPX_CBR) {
      vpx_codec_control(codec.get(), VP9E_SET_AQ_MODE, 3);
    }
  }

  // Tune configs for screen sharing. The values are the same as libwebrtc.
  // third_party/webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.cc.
  // third_party/webrtc/modules/video_coding/codecs/vp9/libvpx_vp9_encoder.cc.
  unsigned int static_thresh = 1;
  if (options.content_hint == ContentHint::Screen) {
    if (is_vp9) {
      // TODO(bugs.webrtc.org/15785): Set static threshold for screen content
      // in VP9 too.
      vpx_codec_control(codec.get(), VP9E_SET_TUNE_CONTENT,
                        VP9E_CONTENT_SCREEN);
    } else {
      static_thresh = 100;
      // Tune configs for screen sharing. The values are the same as WebRTC
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.cc
      // 0: camera (default)
      // 1: screen
      // 2: screen with allowing drop frame.
      unsigned int screen_content_mode = 1;
      if (options.latency_mode == LatencyMode::Realtime &&
          base::FeatureList::IsEnabled(kWebCodecsVideoEncoderFrameDrop)) {
        screen_content_mode = 2;
      }
      vpx_codec_control(codec.get(), VP8E_SET_SCREEN_CONTENT_MODE,
                        screen_content_mode);
    }
  }
  vpx_codec_control(codec.get(), VP8E_SET_STATIC_THRESHOLD, static_thresh);

  options_ = options;
  originally_configured_size_ = options.frame_size;
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  codec_ = std::move(codec);

  if (info_cb) {
    VideoEncoderInfo info;
    info.implementation_name = "VpxVideoEncoder";
    info.is_hardware_accelerated = false;
    BindCallbackToCurrentLoopIfNeeded(std::move(info_cb)).Run(info);
  }

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void VpxVideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                             const EncodeOptions& encode_options,
                             EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  bool key_frame = encode_options.key_frame;
  if (!frame) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kInvalidInputFrame,
                      "No frame provided for encoding."));
    return;
  }

  if (frame->format() == PIXEL_FORMAT_NV12 && frame->HasMappableGpuBuffer()) {
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
  // form needed by libvpx for encoding.
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

  // Resizing should have been taken care of above.
  DCHECK_EQ(frame->visible_rect().size(), options_.frame_size);
  auto planes = base::span(vpx_image_.planes);
  auto stride = base::span(vpx_image_.stride);
  switch (profile_) {
    case VP8PROFILE_ANY:
    case VP9PROFILE_PROFILE0: {
      DCHECK(frame->format() == PIXEL_FORMAT_NV12 ||
             frame->format() == PIXEL_FORMAT_I420);
      if (frame->format() == PIXEL_FORMAT_NV12) {
        RecreateVpxImageIfNeeded(VPX_IMG_FMT_NV12, /*needs_memory=*/false);
        planes[VPX_PLANE_Y] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kY));
        planes[VPX_PLANE_U] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kUV));
        // SAFETY: In NV12 U and V samples are combined in one
        // plane (bytes go UVUVUV), but libvpx treats them as two planes with
        // the same stride but shifted by one byte.
        planes[VPX_PLANE_V] = UNSAFE_BUFFERS(planes[VPX_PLANE_U] + 1);
        stride[VPX_PLANE_Y] = frame->stride(VideoFrame::Plane::kY);
        stride[VPX_PLANE_U] = frame->stride(VideoFrame::Plane::kUV);
        stride[VPX_PLANE_V] = frame->stride(VideoFrame::Plane::kUV);
      } else {
        RecreateVpxImageIfNeeded(VPX_IMG_FMT_I420, /*needs_memory=*/false);
        SetupStandardYuvPlanes(*frame, &vpx_image_);
      }
      break;
    }
    case VP9PROFILE_PROFILE2:
      DCHECK(frame->format() == PIXEL_FORMAT_YUV420P10 ||
             frame->format() == PIXEL_FORMAT_I420);
      if (frame->format() == PIXEL_FORMAT_YUV420P10) {
        RecreateVpxImageIfNeeded(VPX_IMG_FMT_I42016, /*needs_memory=*/false);
        SetupStandardYuvPlanes(*frame, &vpx_image_);
        break;
      }
      RecreateVpxImageIfNeeded(VPX_IMG_FMT_I42016, /*needs_memory=*/true);
      libyuv::I420ToI010(frame->visible_data(VideoFrame::Plane::kY),
                         frame->stride(VideoFrame::Plane::kY),
                         frame->visible_data(VideoFrame::Plane::kU),
                         frame->stride(VideoFrame::Plane::kU),
                         frame->visible_data(VideoFrame::Plane::kV),
                         frame->stride(VideoFrame::Plane::kV),
                         reinterpret_cast<uint16_t*>(planes[VPX_PLANE_Y]),
                         stride[VPX_PLANE_Y] / 2,
                         reinterpret_cast<uint16_t*>(planes[VPX_PLANE_U]),
                         stride[VPX_PLANE_U] / 2,
                         reinterpret_cast<uint16_t*>(planes[VPX_PLANE_V]),
                         stride[VPX_PLANE_V] / 2, frame->visible_rect().width(),
                         frame->visible_rect().height());
      break;

    case VP9PROFILE_PROFILE1:
      DCHECK_EQ(frame->format(), PIXEL_FORMAT_I444);
      RecreateVpxImageIfNeeded(VPX_IMG_FMT_I444, /*needs_memory=*/false);
      SetupStandardYuvPlanes(*frame, &vpx_image_);
      break;

    case VP9PROFILE_PROFILE3:
      DCHECK(frame->format() == PIXEL_FORMAT_YUV444P10 ||
             frame->format() == PIXEL_FORMAT_I444);
      if (frame->format() == PIXEL_FORMAT_YUV444P10) {
        RecreateVpxImageIfNeeded(VPX_IMG_FMT_I44416, /*needs_memory=*/false);
        SetupStandardYuvPlanes(*frame, &vpx_image_);
        break;
      }
      RecreateVpxImageIfNeeded(VPX_IMG_FMT_I44416, /*needs_memory=*/true);
      I444ToI410(*frame, &vpx_image_);
      break;

    default:
      NOTREACHED();  // Checked during Initialize().
  }

  // Use zero as a timestamp, so encoder will not use it for rate control.
  // In absence of timestamp libvpx uses duration.
  constexpr auto timestamp_us = 0;
  auto duration_us = GetFrameDuration(*frame).InMicroseconds();
  last_frame_timestamp_ = frame->timestamp();
  if (last_frame_color_space_ != frame->ColorSpace()) {
    last_frame_color_space_ = frame->ColorSpace();
    key_frame = true;
    UpdateEncoderColorSpace();
  }
  const unsigned long deadline = VPX_DL_REALTIME;
  vpx_codec_flags_t flags = key_frame ? VPX_EFLAG_FORCE_KF : 0;

  int temporal_id = 0;
  const bool is_layer_encoding = codec_config_.ts_number_layers > 1;
  if (is_layer_encoding) {
    if (key_frame)
      temporal_svc_frame_index_ = 0;
    unsigned int index_in_temp_cycle =
        temporal_svc_frame_index_ % codec_config_.ts_periodicity;
    temporal_id = base::span(codec_config_.ts_layer_id)[index_in_temp_cycle];
    if (profile_ == VP8PROFILE_ANY) {
      auto vp8_layers_flags =
          codec_config_.ts_number_layers == 2
              ? base::span<vpx_enc_frame_flags_t>(vp8_2layers_temporal_flags)
              : base::span<vpx_enc_frame_flags_t>(vp8_3layers_temporal_flags);
      flags |= vp8_layers_flags[index_in_temp_cycle];
      vpx_codec_control(codec_.get(), VP8E_SET_TEMPORAL_LAYER_ID, temporal_id);
    }
  }

  if (encode_options.quantizer.has_value()) {
    DCHECK_EQ(options_.bitrate->mode(), Bitrate::Mode::kExternal);
    // Convert double quantizer to an integer within codec's supported range.
    int qp = static_cast<int>(std::lround(encode_options.quantizer.value()));
    qp = std::clamp(qp, static_cast<int>(codec_config_.rc_min_quantizer),
                    static_cast<int>(codec_config_.rc_max_quantizer));
    vpx_codec_control(codec_.get(), VP9E_SET_QUANTIZER_ONE_PASS, qp);
  }

  TRACE_EVENT1("media", "vpx_codec_encode", "timestamp", frame->timestamp());
  auto vpx_error = vpx_codec_encode(codec_.get(), &vpx_image_, timestamp_us,
                                    duration_us, flags, deadline);

  if (vpx_error != VPX_CODEC_OK) {
    auto msg =
        LogVpxErrorMessage(codec_.get(), "VPX encoding error", vpx_error);
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode, msg)
            .WithData("vpx_error", vpx_error));
    return;
  }

  auto output =
      GetEncoderOutput(temporal_id, frame->timestamp(), frame->ColorSpace());
  if (is_layer_encoding) {
    // If we got an unexpected key frame, |temporal_svc_frame_index_| needs to
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

void VpxVideoEncoder::ChangeOptions(const Options& options,
                                    OutputCB output_cb,
                                    EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  // Libvpx is very peculiar about encoded frame size changes,
  // - VP8: vpx_codec_enc_config_set() returns VPX_CODEC_INVALID_PARAM if we
  //        try to increase encoded width or height larger than their initial
  //        values.
  // - VP9: The codec may crash if we try to increase encoded width or height
  //        larger than their initial values.
  //
  // Mind the difference between old frame sizes:
  // - |originally_configured_size_| is set only once when the vpx_codec_ctx_t
  // is created.
  // - |options_.frame_size| changes every time ChangeOptions() is called.
  // More info can be found here:
  //   https://bugs.chromium.org/p/webm/issues/detail?id=1642
  //   https://bugs.chromium.org/p/webm/issues/detail?id=912
  if (profile_ != VP8PROFILE_ANY) {
    // VP9 resize restrictions
    if (options.frame_size.width() > originally_configured_size_.width() ||
        options.frame_size.height() > originally_configured_size_.height()) {
      auto status = EncoderStatus(
          EncoderStatus::Codes::kEncoderUnsupportedConfig,
          "libvpx/VP9 doesn't support dynamically increasing frame dimensions");
      std::move(done_cb).Run(std::move(status));
      return;
    }
  }

  vpx_codec_enc_cfg_t new_config = codec_config_;
  auto status = SetUpVpxConfig(options, profile_, &new_config);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  auto error = vpx_codec_enc_config_set(codec_.get(), &new_config);
  const bool is_vp9 = (profile_ != VP8PROFILE_ANY);
  if (is_vp9 && error == VPX_CODEC_OK && new_config.ts_number_layers > 1) {
    vpx_svc_extra_cfg_t svc_conf = MakeSvcExtraConfig(new_config);
    vpx_codec_control(codec_.get(), VP9E_SET_SVC_PARAMETERS, &svc_conf);
    error = vpx_codec_control(codec_.get(), VP9E_SET_SVC, 1);
  }
  if (error == VPX_CODEC_OK) {
    codec_config_ = new_config;
    options_ = options;
    if (!output_cb.is_null())
      output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  } else {
    status = EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                           "Failed to set new VPX config")
                 .WithData("vpx_error", error);
  }

  std::move(done_cb).Run(std::move(status));
}

base::TimeDelta VpxVideoEncoder::GetFrameDuration(const VideoFrame& frame) {
  // Frame has duration in metadata, use it.
  if (frame.metadata().frame_duration.has_value())
    return frame.metadata().frame_duration.value();

  // Options have framerate specified, use it.
  if (options_.framerate.has_value())
    return base::Seconds(1.0 / options_.framerate.value());

  // No real way to figure out duration, use time passed since the last frame
  // as an educated guess, but clamp it within a reasonable limits.
  constexpr auto min_duration = base::Seconds(1.0 / 60.0);
  constexpr auto max_duration = base::Seconds(1.0 / 24.0);
  auto duration = frame.timestamp() - last_frame_timestamp_;
  return std::clamp(duration, min_duration, max_duration);
}

VpxVideoEncoder::~VpxVideoEncoder() {
  if (!codec_)
    return;

  // It's safe to call vpx_img_free, even if vpx_image_ has never been
  // initialized. vpx_img_free is not going to deallocate the vpx_image_
  // itself, only internal buffers.
  vpx_img_free(&vpx_image_);
}

void VpxVideoEncoder::Flush(EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  // The libvpx encoder is operating synchronously and thus doesn't have to
  // flush if and only if |g_lag_in_frames| is set to 0.
  CHECK_EQ(codec_config_.g_lag_in_frames, 0u);
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

VideoEncoderOutput VpxVideoEncoder::GetEncoderOutput(
    int temporal_id,
    base::TimeDelta timestamp,
    gfx::ColorSpace color_space) const {
  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  VideoEncoderOutput output;
  // We don't given timestamps to vpx_codec_encode() that's why
  // pkt->data.frame.pts can't be used here.
  output.timestamp = timestamp;
  while ((pkt = vpx_codec_get_cx_data(codec_.get(), &iter))) {
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      // The encoder is operating synchronously. There should be exactly one
      // encoded packet, or the frame is dropped.
      // SAFETY: It's libvpx documented behaviour that pkt->data.frame.buf
      // has size of pkt->data.frame.sz.
      output.data = base::HeapArray<uint8_t>::CopiedFrom(
          UNSAFE_BUFFERS({reinterpret_cast<uint8_t*>(pkt->data.frame.buf),
                          pkt->data.frame.sz}));
      output.key_frame = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
      output.temporal_id = output.key_frame ? 0 : temporal_id;
      output.color_space = color_space;
    }
  }
  return output;
}

void VpxVideoEncoder::RecreateVpxImageIfNeeded(vpx_img_fmt fmt,
                                               bool needs_memory) {
  const bool has_changed = vpx_image_.fmt != fmt ||
                           vpx_image_.d_w != codec_config_.g_w ||
                           vpx_image_.d_h != codec_config_.g_h;

  if (!has_changed) {
    return;
  }

  vpx_img_free(&vpx_image_);
  if (needs_memory) {
    CHECK(vpx_img_alloc(&vpx_image_, fmt, codec_config_.g_w, codec_config_.g_h,
                        /*align=*/1));
  } else {
    // Encoding will write the actual plane pointers, but we have to pass a
    // value to vpx_img_wrap() to avoid an unnecessary allocation.
    static const uint8_t unused = 0;
    CHECK(vpx_img_wrap(&vpx_image_, fmt, codec_config_.g_w, codec_config_.g_h,
                       /*stride_align=*/1, const_cast<uint8_t*>(&unused)));
  }

  vpx_image_.bit_depth = (fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 16 : 8;
}

void VpxVideoEncoder::UpdateEncoderColorSpace() {
  auto vpx_cs = VPX_CS_UNKNOWN;
  switch (last_frame_color_space_.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709: {
      const auto matrix_id = last_frame_color_space_.GetMatrixID();
      if (matrix_id == gfx::ColorSpace::MatrixID::GBR ||
          matrix_id == gfx::ColorSpace::MatrixID::RGB) {
        vpx_cs = VPX_CS_SRGB;
      } else {
        vpx_cs = VPX_CS_BT_709;
      }
      break;
    }
    case gfx::ColorSpace::PrimaryID::BT2020:
      vpx_cs = VPX_CS_BT_2020;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      vpx_cs = VPX_CS_SMPTE_170;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      vpx_cs = VPX_CS_SMPTE_240;
      break;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      vpx_cs = VPX_CS_BT_601;
      break;
    default:
      break;
  };

  if (vpx_cs != VPX_CS_UNKNOWN) {
    vpx_image_.cs = vpx_cs;
    if (profile_ != VP8PROFILE_ANY) {
      auto vpx_error =
          vpx_codec_control(codec_.get(), VP9E_SET_COLOR_SPACE, vpx_cs);
      if (vpx_error != VPX_CODEC_OK) {
        LogVpxErrorMessage(codec_.get(), "Failed to set color space",
                           vpx_error);
      }
    }
  }

  if (last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL ||
      last_frame_color_space_.GetRangeID() ==
          gfx::ColorSpace::RangeID::LIMITED) {
    const auto vpx_range =
        last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL
            ? VPX_CR_FULL_RANGE
            : VPX_CR_STUDIO_RANGE;
    vpx_image_.range = vpx_range;
    if (profile_ != VP8PROFILE_ANY) {
      auto vpx_error =
          vpx_codec_control(codec_.get(), VP9E_SET_COLOR_RANGE, vpx_range);
      if (vpx_error != VPX_CODEC_OK) {
        LogVpxErrorMessage(codec_.get(), "Failed to set color range",
                           vpx_error);
      }
    }
  }
}

}  // namespace media
