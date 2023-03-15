// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/vpx_video_encoder.h"

#include <algorithm>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/timestamp_constants.h"
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
                             vpx_codec_enc_cfg_t* config) {
  if (opts.frame_size.width() <= 0 || opts.frame_size.height() <= 0)
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Negative width or height values.");

  if (!opts.frame_size.GetCheckedArea().IsValid())
    return EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                         "Frame is too large.");

  config->g_pass = VPX_RC_ONE_PASS;
  config->g_lag_in_frames = 0;
  config->rc_max_quantizer = 58;
  config->rc_min_quantizer = 2;
  config->rc_resize_allowed = 0;
  config->rc_dropframe_thresh = 0;  // Don't drop frames
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

  if (opts.bitrate.has_value()) {
    auto& bitrate = opts.bitrate.value();
    config->rc_target_bitrate = bitrate.target_bps() / 1000;
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
  } else {
    config->rc_target_bitrate = GetDefaultVideoEncodeBitrate(
        opts.frame_size, opts.framerate.value_or(30));
  }

  config->g_w = opts.frame_size.width();
  config->g_h = opts.frame_size.height();

  if (!opts.scalability_mode)
    return EncoderStatus::Codes::kOk;

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
      config->ts_layer_id[0] = 0;
      config->ts_layer_id[1] = 1;
      config->ts_rate_decimator[0] = 2;
      config->ts_rate_decimator[1] = 1;
      // Bitrate allocation L0: 60% L1: 40%
      config->layer_target_bitrate[0] = config->ts_target_bitrate[0] =
          60 * config->rc_target_bitrate / 100;
      config->layer_target_bitrate[1] = config->ts_target_bitrate[1] =
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
      config->ts_layer_id[0] = 0;
      config->ts_layer_id[1] = 2;
      config->ts_layer_id[2] = 1;
      config->ts_layer_id[3] = 2;
      config->ts_rate_decimator[0] = 4;
      config->ts_rate_decimator[1] = 2;
      config->ts_rate_decimator[2] = 1;
      // Bitrate allocation L0: 50% L1: 20% L2: 30%
      config->layer_target_bitrate[0] = config->ts_target_bitrate[0] =
          50 * config->rc_target_bitrate / 100;
      config->layer_target_bitrate[1] = config->ts_target_bitrate[1] =
          70 * config->rc_target_bitrate / 100;
      config->layer_target_bitrate[2] = config->ts_target_bitrate[2] =
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
  for (size_t i = 0; i < config.ts_number_layers; ++i) {
    result.scaling_factor_num[i] = 1;
    result.scaling_factor_den[i] = 1;
    result.max_quantizers[i] = config.rc_max_quantizer;
    result.min_quantizers[i] = config.rc_min_quantizer;
  }
  return result;
}

EncoderStatus ReallocateVpxImageIfNeeded(vpx_image_t* vpx_image,
                                         const vpx_img_fmt fmt,
                                         int width,
                                         int height) {
  if (vpx_image->fmt != fmt || static_cast<int>(vpx_image->w) != width ||
      static_cast<int>(vpx_image->h) != height) {
    vpx_img_free(vpx_image);
    if (vpx_image != vpx_img_alloc(vpx_image, fmt, width, height, 1)) {
      return EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                           "Invalid format or frame size.");
    }
    vpx_image->bit_depth = (fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 16 : 8;
  }
  // else no-op since the image don't need to change format.
  return EncoderStatus::Codes::kOk;
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
  } else if (profile == VP9PROFILE_PROFILE0 || profile == VP9PROFILE_PROFILE2) {
    // TODO(https://crbug.com/1116617): Consider support for profiles 1 and 3.
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
                      "Unsupported bitrate mode"));
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

  vpx_img_fmt img_fmt = VPX_IMG_FMT_NONE;
  unsigned int bits_for_storage = 8;
  switch (profile) {
    case VP9PROFILE_PROFILE1:
      codec_config_.g_profile = 1;
      break;
    case VP9PROFILE_PROFILE2:
      codec_config_.g_profile = 2;
      img_fmt = VPX_IMG_FMT_I42016;
      bits_for_storage = 16;
      codec_config_.g_bit_depth = VPX_BITS_10;
      codec_config_.g_input_bit_depth = 10;
      break;
    case VP9PROFILE_PROFILE3:
      codec_config_.g_profile = 3;
      break;
    default:
      codec_config_.g_profile = 0;
      img_fmt = VPX_IMG_FMT_I420;
      bits_for_storage = 8;
      codec_config_.g_bit_depth = VPX_BITS_8;
      codec_config_.g_input_bit_depth = 8;
      break;
  }

  auto status = SetUpVpxConfig(options, &codec_config_);
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
  // For VP8 typical values used for real-time encoding are -4, -6,
  // -8, -10. Again larger magnitude means faster encoding but lower
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

  if (&vpx_image_ != vpx_img_alloc(&vpx_image_, img_fmt,
                                   options.frame_size.width(),
                                   options.frame_size.height(), 1)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Invalid format or frame size."));
    return;
  }
  vpx_image_.bit_depth = bits_for_storage;

  if (is_vp9) {
    // Set the number of column tiles in encoding an input frame, with number of
    // tile columns (in Log2 unit) as the parameter.
    // The minimum width of a tile column is 256 pixels, the maximum is 4096.
    int log2_tile_columns =
        static_cast<int>(std::log2(codec_config_.g_w / 256));
    vpx_codec_control(codec.get(), VP9E_SET_TILE_COLUMNS, log2_tile_columns);

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

    // In CBR mode use aq-mode=3 is enabled for quality improvement
    if (codec_config_.rc_end_usage == VPX_CBR) {
      vpx_codec_control(codec.get(), VP9E_SET_AQ_MODE, 3);
    }
  }

  options_ = options;
  originally_configured_size_ = options.frame_size;
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  codec_ = std::move(codec);

  VideoEncoderInfo info;
  info.implementation_name = "VpxVideoEncoder";
  info.is_hardware_accelerated = false;
  BindCallbackToCurrentLoopIfNeeded(std::move(info_cb)).Run(info);

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
            .WithData("format", frame->format()));
    return;
  }

  if (frame->format() == PIXEL_FORMAT_NV12 && frame->HasGpuMemoryBuffer()) {
    frame = ConvertToMemoryMappedFrame(frame);
    if (!frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Convert GMB frame to MemoryMappedFrame failed."));
      return;
    }
  }

  // Unfortunately libyuv lacks direct NV12 to I010 conversion, and we
  // have to do an extra conversion to I420.
  // TODO(https://crbug.com/libyuv/954) Use NV12ToI010() when implemented
  const bool vp9_p2_needs_nv12_to_i420 =
      frame->format() == PIXEL_FORMAT_NV12 && profile_ == VP9PROFILE_PROFILE2;
  const bool needs_conversion_to_i420 =
      !IsYuvPlanar(frame->format()) || vp9_p2_needs_nv12_to_i420;
  if (frame->visible_rect().size() != options_.frame_size ||
      needs_conversion_to_i420) {
    auto new_pixel_format =
        needs_conversion_to_i420 ? PIXEL_FORMAT_I420 : frame->format();
    auto resized_frame = frame_pool_.CreateFrame(
        new_pixel_format, options_.frame_size, gfx::Rect(options_.frame_size),
        options_.frame_size, frame->timestamp());

    if (!resized_frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Can't allocate a resized frame"));
      return;
    }

    auto convert_status =
        ConvertAndScaleFrame(*frame, *resized_frame, resize_buf_);
    if (!convert_status.is_ok()) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode)
              .AddCause(std::move(convert_status)));
      return;
    }
    frame = std::move(resized_frame);
  }

  switch (profile_) {
    case VP9PROFILE_PROFILE2:
      DCHECK_EQ(frame->format(), PIXEL_FORMAT_I420);
      // Profile 2 uses 10bit color,
      libyuv::I420ToI010(
          frame->visible_data(VideoFrame::kYPlane),
          frame->stride(VideoFrame::kYPlane),
          frame->visible_data(VideoFrame::kUPlane),
          frame->stride(VideoFrame::kUPlane),
          frame->visible_data(VideoFrame::kVPlane),
          frame->stride(VideoFrame::kVPlane),
          reinterpret_cast<uint16_t*>(vpx_image_.planes[VPX_PLANE_Y]),
          vpx_image_.stride[VPX_PLANE_Y] / 2,
          reinterpret_cast<uint16_t*>(vpx_image_.planes[VPX_PLANE_U]),
          vpx_image_.stride[VPX_PLANE_U] / 2,
          reinterpret_cast<uint16_t*>(vpx_image_.planes[VPX_PLANE_V]),
          vpx_image_.stride[VPX_PLANE_V] / 2, frame->visible_rect().width(),
          frame->visible_rect().height());
      break;
    case VP9PROFILE_PROFILE1:
    case VP9PROFILE_PROFILE3:
      NOTREACHED();
      break;
    default:
      vpx_img_fmt_t fmt = frame->format() == PIXEL_FORMAT_NV12
                              ? VPX_IMG_FMT_NV12
                              : VPX_IMG_FMT_I420;
      EncoderStatus status = ReallocateVpxImageIfNeeded(
          &vpx_image_, fmt, codec_config_.g_w, codec_config_.g_h);
      if (!status.is_ok()) {
        std::move(done_cb).Run(status);
        return;
      }
      if (fmt == VPX_IMG_FMT_NV12) {
        vpx_image_.planes[VPX_PLANE_Y] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::kYPlane));
        vpx_image_.planes[VPX_PLANE_U] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::kUVPlane));
        // In NV12 Y and U samples are combined in one plane (bytes go YUYUYU),
        // but libvpx treats them as two planes with the same stride but shifted
        // by one byte.
        vpx_image_.planes[VPX_PLANE_V] = vpx_image_.planes[VPX_PLANE_U] + 1;
        vpx_image_.stride[VPX_PLANE_Y] = frame->stride(VideoFrame::kYPlane);
        vpx_image_.stride[VPX_PLANE_U] = frame->stride(VideoFrame::kUVPlane);
        vpx_image_.stride[VPX_PLANE_V] = frame->stride(VideoFrame::kUVPlane);
      } else {
        vpx_image_.planes[VPX_PLANE_Y] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::kYPlane));
        vpx_image_.planes[VPX_PLANE_U] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::kUPlane));
        vpx_image_.planes[VPX_PLANE_V] =
            const_cast<uint8_t*>(frame->visible_data(VideoFrame::kVPlane));
        vpx_image_.stride[VPX_PLANE_Y] = frame->stride(VideoFrame::kYPlane);
        vpx_image_.stride[VPX_PLANE_U] = frame->stride(VideoFrame::kUPlane);
        vpx_image_.stride[VPX_PLANE_V] = frame->stride(VideoFrame::kVPlane);
      }
      break;
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
  auto deadline = VPX_DL_REALTIME;
  vpx_codec_flags_t flags = key_frame ? VPX_EFLAG_FORCE_KF : 0;

  int temporal_id = 0;
  if (codec_config_.ts_number_layers > 1) {
    if (key_frame)
      temporal_svc_frame_index = 0;
    int index_in_temp_cycle =
        temporal_svc_frame_index % codec_config_.ts_periodicity;
    temporal_id = codec_config_.ts_layer_id[index_in_temp_cycle];
    temporal_svc_frame_index++;

    if (profile_ == VP8PROFILE_ANY) {
      auto* vp8_layers_flags = codec_config_.ts_number_layers == 2
                                   ? vp8_2layers_temporal_flags
                                   : vp8_3layers_temporal_flags;
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

  DrainOutputs(temporal_id, frame->timestamp(), frame->ColorSpace());
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
  // - VP8: As long as the frame area doesn't increase, internal codec
  //        structures don't need to be reallocated and codec can be simply
  //        reconfigured.
  // - VP9: The codec cannot increase encoded width or height larger than their
  //        initial values.
  //
  // Mind the difference between old frame sizes:
  // - |originally_configured_size_| is set only once when the vpx_codec_ctx_t
  // is created.
  // - |options_.frame_size| changes every time ChangeOptions() is called.
  // More info can be found here:
  //   https://bugs.chromium.org/p/webm/issues/detail?id=1642
  //   https://bugs.chromium.org/p/webm/issues/detail?id=912
  if (profile_ == VP8PROFILE_ANY) {
    // VP8 resize restrictions
    auto old_area = originally_configured_size_.GetCheckedArea();
    auto new_area = options.frame_size.GetCheckedArea();
    DCHECK(old_area.IsValid());
    if (!new_area.IsValid() || new_area.ValueOrDie() > old_area.ValueOrDie()) {
      auto status = EncoderStatus(
          EncoderStatus::Codes::kEncoderUnsupportedConfig,
          "libvpx/VP8 doesn't support dynamically increasing frame area");
      std::move(done_cb).Run(std::move(status));
      return;
    }
  } else {
    // VP9 resize restrictions
    if (options.frame_size.width() > originally_configured_size_.width() ||
        options.frame_size.height() > originally_configured_size_.height()) {
      auto status = EncoderStatus(
          EncoderStatus::Codes::kEncoderUnsupportedConfig,
          "libvpx/VP9 doesn't support dynamically increasing frame dimentions");
      std::move(done_cb).Run(std::move(status));
      return;
    }
  }

  vpx_codec_enc_cfg_t new_config = codec_config_;
  auto status = SetUpVpxConfig(options, &new_config);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  status = ReallocateVpxImageIfNeeded(&vpx_image_, vpx_image_.fmt,
                                      options.frame_size.width(),
                                      options.frame_size.height());
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
  return base::clamp(duration, min_duration, max_duration);
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

  auto vpx_error = vpx_codec_encode(codec_.get(), nullptr, -1, 0, 0, 0);
  if (vpx_error != VPX_CODEC_OK) {
    auto msg =
        LogVpxErrorMessage(codec_.get(), "VPX flushing error", vpx_error);
    auto status = EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode, msg)
                      .WithData("vpx_error", vpx_error);
    std::move(done_cb).Run(std::move(status));
    return;
  }
  DrainOutputs(0, base::TimeDelta(), gfx::ColorSpace());
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void VpxVideoEncoder::DrainOutputs(int temporal_id,
                                   base::TimeDelta ts,
                                   gfx::ColorSpace color_space) {
  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  while ((pkt = vpx_codec_get_cx_data(codec_.get(), &iter))) {
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      VideoEncoderOutput result;
      result.key_frame = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;

      if (result.key_frame) {
        // If we got an unexpected key frame, temporal_svc_frame_index needs to
        // be adjusted, because the next frame should have index 1.
        temporal_svc_frame_index = 1;
        result.temporal_id = 0;
      } else {
        result.temporal_id = temporal_id;
      }

      // We don't given timestamps to vpx_codec_encode() that's why
      // pkt->data.frame.pts can't be used here.
      result.timestamp = ts;
      result.color_space = color_space;
      result.size = pkt->data.frame.sz;
      result.data = std::make_unique<uint8_t[]>(result.size);
      memcpy(result.data.get(), pkt->data.frame.buf, result.size);
      output_cb_.Run(std::move(result), {});
    }
  }
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
    auto vpx_error =
        vpx_codec_control(codec_.get(), VP9E_SET_COLOR_SPACE, vpx_cs);
    if (vpx_error != VPX_CODEC_OK)
      LogVpxErrorMessage(codec_.get(), "Failed to set color space", vpx_error);
  }

  if (last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL ||
      last_frame_color_space_.GetRangeID() ==
          gfx::ColorSpace::RangeID::LIMITED) {
    auto vpx_error = vpx_codec_control(
        codec_.get(), VP9E_SET_COLOR_RANGE,
        last_frame_color_space_.GetRangeID() == gfx::ColorSpace::RangeID::FULL
            ? VPX_CR_FULL_RANGE
            : VPX_CR_STUDIO_RANGE);
    if (vpx_error != VPX_CODEC_OK)
      LogVpxErrorMessage(codec_.get(), "Failed to set color range", vpx_error);
  }
}

}  // namespace media
