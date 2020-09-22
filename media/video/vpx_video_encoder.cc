// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/vpx_video_encoder.h"

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace media {

namespace {

// Returns the number of threads.
int GetNumberOfThreads(int width) {
  // Default to 1 thread for less than VGA.
  int desired_threads = 1;

  if (width >= 3840)
    desired_threads = 16;
  else if (width >= 2560)
    desired_threads = 8;
  else if (width >= 1280)
    desired_threads = 4;
  else if (width >= 640)
    desired_threads = 2;

  // Clamp to the number of available logical processors/cores.
  desired_threads =
      std::min(desired_threads, base::SysInfo::NumberOfProcessors());

  return desired_threads;
}

Status SetUpVpxConfig(const VideoEncoder::Options& opts,
                      vpx_codec_enc_cfg_t* config) {
  if (opts.width <= 0 || opts.height <= 0)
    return Status(StatusCode::kEncoderUnsupportedConfig,
                  "Negative width or height values");

  config->g_pass = VPX_RC_ONE_PASS;
  config->g_lag_in_frames = 0;
  config->rc_resize_allowed = 0;
  config->rc_dropframe_thresh = 0;  // Don't drop frames
  config->g_timebase.num = 1;
  config->g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // Set the number of threads based on the image width and num of cores.
  config->g_threads = GetNumberOfThreads(opts.width);

  // Insert keyframes at will with a given max interval
  if (opts.keyframe_interval.has_value()) {
    config->kf_mode = VPX_KF_AUTO;
    config->kf_min_dist = 0;
    config->kf_max_dist = opts.keyframe_interval.value();
  }

  if (opts.bitrate.has_value() && opts.bitrate.value()) {
    config->rc_end_usage = VPX_CBR;
    config->rc_target_bitrate = opts.bitrate.value() / 1000;
  } else {
    config->rc_end_usage = VPX_VBR;
    config->rc_target_bitrate = double{opts.width} * double{opts.height} /
                                config->g_w / config->g_h *
                                config->rc_target_bitrate;
  }

  config->g_w = opts.width;
  config->g_h = opts.height;

  return Status();
}

void FreeCodecCtx(vpx_codec_ctx_t* codec_ctx) {
  if (codec_ctx->name) {
    // Codec has been initialized, we need to destroy it.
    auto error = vpx_codec_destroy(codec_ctx);
    DCHECK_EQ(error, VPX_CODEC_OK);
  }

  delete codec_ctx;
}

}  // namespace

VpxVideoEncoder::VpxVideoEncoder() : codec_(nullptr, FreeCodecCtx) {}

void VpxVideoEncoder::Initialize(VideoCodecProfile profile,
                                 const Options& options,
                                 OutputCB output_cb,
                                 StatusCB done_cb) {
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeTwice);
    return;
  }

  profile_ = profile;

  vpx_codec_iface_t* iface = nullptr;
  if (profile == media::VP8PROFILE_ANY) {
    iface = vpx_codec_vp8_cx();
  } else if (profile == media::VP9PROFILE_PROFILE0 ||
             profile == media::VP9PROFILE_PROFILE2) {
    // TODO(https://crbug.com/1116617): Consider support for profiles 1 and 3.
    is_vp9_ = true;
    iface = vpx_codec_vp9_cx();
  } else {
    auto status = Status(StatusCode::kEncoderUnsupportedProfile)
                      .WithData("profile", profile);
    std::move(done_cb).Run(status);
    return;
  }

  auto vpx_error = vpx_codec_enc_config_default(iface, &codec_config_, 0);
  if (vpx_error != VPX_CODEC_OK) {
    auto status = Status(StatusCode::kEncoderInitializationError,
                         "Failed to get default VPX config.")
                      .WithData("vpx_error", vpx_error);
    std::move(done_cb).Run(status);
    return;
  }

  vpx_img_fmt img_fmt = VPX_IMG_FMT_NONE;
  unsigned int bits_for_storage = 8;
  switch (profile) {
    case media::VP9PROFILE_PROFILE1:
      codec_config_.g_profile = 1;
      break;
    case media::VP9PROFILE_PROFILE2:
      codec_config_.g_profile = 2;
      img_fmt = VPX_IMG_FMT_I42016;
      bits_for_storage = 16;
      codec_config_.g_bit_depth = VPX_BITS_10;
      codec_config_.g_input_bit_depth = 10;
      break;
    case media::VP9PROFILE_PROFILE3:
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

  Status status = SetUpVpxConfig(options, &codec_config_);
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
    std::string msg = base::StringPrintf(
        "VPX encoder initialization error: %s %s",
        vpx_codec_err_to_string(vpx_error), codec->err_detail);

    status = Status(StatusCode::kEncoderInitializationError, msg);
    std::move(done_cb).Run(status);
    return;
  }

  // Due to https://bugs.chromium.org/p/webm/issues/detail?id=1684
  // values less than 5 crash VP9 encoder.
  vpx_error = vpx_codec_control(codec.get(), VP8E_SET_CPUUSED, 5);
  if (vpx_error != VPX_CODEC_OK) {
    std::string msg =
        base::StringPrintf("VPX encoder VP8E_SET_CPUUSED error: %s",
                           vpx_codec_err_to_string(vpx_error));

    status = Status(StatusCode::kEncoderInitializationError, msg);
    std::move(done_cb).Run(status);
    return;
  }

  if (&vpx_image_ != vpx_img_wrap(&vpx_image_, img_fmt, options.width,
                                  options.height, 1, nullptr)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Invalid format or frame size.");
    std::move(done_cb).Run(status);
    return;
  }
  vpx_image_.bit_depth = bits_for_storage;

  if (is_vp9_) {
    // Set the number of column tiles in encoding an input frame, with number of
    // tile columns (in Log2 unit) as the parameter.
    // The minimum width of a tile column is 256 pixels, the maximum is 4096.
    int log2_tile_columns =
        static_cast<int>(std::log2(codec_config_.g_w / 256));
    vpx_codec_control(codec.get(), VP9E_SET_TILE_COLUMNS, log2_tile_columns);

    // Turn on row level multi-threading.
    vpx_codec_control(codec.get(), VP9E_SET_ROW_MT, 1);
  }

  options_ = options;
  output_cb_ = media::BindToCurrentLoop(std::move(output_cb));
  codec_ = std::move(codec);
  std::move(done_cb).Run(Status());
}

void VpxVideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                             bool key_frame,
                             StatusCB done_cb) {
  Status status;
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  if (!frame) {
    std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                  "No frame provided for encoding."));
    return;
  }
  if (!frame->IsMappable() || frame->format() != media::PIXEL_FORMAT_I420) {
    status =
        Status(StatusCode::kEncoderFailedEncode, "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("format", frame->format());
    std::move(done_cb).Run(std::move(status));
    return;
  }

  switch (profile_) {
    case media::VP9PROFILE_PROFILE1:
      NOTREACHED();
      break;
    case media::VP9PROFILE_PROFILE2:
      libyuv::I420ToI010(
          frame->visible_data(media::VideoFrame::kYPlane),
          frame->stride(media::VideoFrame::kYPlane),
          frame->visible_data(media::VideoFrame::kUPlane),
          frame->stride(media::VideoFrame::kUPlane),
          frame->visible_data(media::VideoFrame::kVPlane),
          frame->stride(media::VideoFrame::kVPlane),
          reinterpret_cast<uint16_t*>(vpx_image_.planes[VPX_PLANE_Y]),
          vpx_image_.stride[VPX_PLANE_Y] / 2,
          reinterpret_cast<uint16_t*>(vpx_image_.planes[VPX_PLANE_U]),
          vpx_image_.stride[VPX_PLANE_U] / 2,
          reinterpret_cast<uint16_t*>(vpx_image_.planes[VPX_PLANE_V]),
          vpx_image_.stride[VPX_PLANE_V] / 2, frame->coded_size().width(),
          frame->coded_size().height());
      break;
    case media::VP9PROFILE_PROFILE3:
      NOTREACHED();
      break;
    default:
      vpx_image_.planes[VPX_PLANE_Y] =
          const_cast<uint8_t*>(frame->visible_data(media::VideoFrame::kYPlane));
      vpx_image_.planes[VPX_PLANE_U] =
          const_cast<uint8_t*>(frame->visible_data(media::VideoFrame::kUPlane));
      vpx_image_.planes[VPX_PLANE_V] =
          const_cast<uint8_t*>(frame->visible_data(media::VideoFrame::kVPlane));
      vpx_image_.stride[VPX_PLANE_Y] =
          frame->stride(media::VideoFrame::kYPlane);
      vpx_image_.stride[VPX_PLANE_U] =
          frame->stride(media::VideoFrame::kUPlane);
      vpx_image_.stride[VPX_PLANE_V] =
          frame->stride(media::VideoFrame::kVPlane);
      break;
  }

  auto timestamp = frame->timestamp().InMicroseconds();
  auto duration = GetFrameDuration(*frame);
  auto deadline = VPX_DL_REALTIME;
  vpx_codec_flags_t flags = key_frame ? VPX_EFLAG_FORCE_KF : 0;
  auto vpx_error = vpx_codec_encode(codec_.get(), &vpx_image_, timestamp,
                                    duration, flags, deadline);

  if (vpx_error != VPX_CODEC_OK) {
    std::string msg = base::StringPrintf("VPX encoding error: %s (%s)",
                                         vpx_codec_err_to_string(vpx_error),
                                         vpx_codec_error_detail(codec_.get()));
    status = Status(StatusCode::kEncoderFailedEncode, msg)
                 .WithData("vpx_error", vpx_error);
    std::move(done_cb).Run(std::move(status));
    return;
  }

  DrainOutputs();
  std::move(done_cb).Run(Status());
}

void VpxVideoEncoder::ChangeOptions(const Options& options, StatusCB done_cb) {
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  vpx_codec_enc_cfg_t new_config = codec_config_;
  auto status = SetUpVpxConfig(options, &new_config);
  if (status.is_ok()) {
    auto vpx_error = vpx_codec_enc_config_set(codec_.get(), &new_config);
    if (vpx_error == VPX_CODEC_OK) {
      codec_config_ = new_config;
      options_ = options;
    } else {
      status = Status(StatusCode::kEncoderUnsupportedConfig,
                      "Failed to set new VPX config")
                   .WithData("vpx_error", vpx_error);
    }
  }

  std::move(done_cb).Run(std::move(status));
  return;
}

uint64_t VpxVideoEncoder::GetFrameDuration(const VideoFrame& frame) {
  base::TimeDelta default_duration =
      base::TimeDelta::FromSecondsD(1.0 / options_.framerate);
  return frame.metadata()
      ->frame_duration.value_or(default_duration)
      .InMicroseconds();
}

VpxVideoEncoder::~VpxVideoEncoder() {
  if (!codec_)
    return;

  // It's safe to call vpx_img_free, even if vpx_image_ has never been
  // initialized. vpx_img_free is not going to deallocate the vpx_image_
  // itself, only internal buffers.
  vpx_img_free(&vpx_image_);
}

void VpxVideoEncoder::Flush(StatusCB done_cb) {
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  auto vpx_error = vpx_codec_encode(codec_.get(), nullptr, -1, 0, 0, 0);
  if (vpx_error != VPX_CODEC_OK) {
    std::string msg = base::StringPrintf("VPX flushing error: %s (%s)",
                                         vpx_codec_err_to_string(vpx_error),
                                         vpx_codec_error_detail(codec_.get()));
    Status status = Status(StatusCode::kEncoderFailedEncode, msg)
                        .WithData("vpx_error", vpx_error);
    std::move(done_cb).Run(std::move(status));
    return;
  }
  DrainOutputs();
  std::move(done_cb).Run(Status());
}

void VpxVideoEncoder::DrainOutputs() {
  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  while ((pkt = vpx_codec_get_cx_data(codec_.get(), &iter))) {
    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      VideoEncoderOutput result;
      result.key_frame = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
      result.timestamp = base::TimeDelta::FromMicroseconds(pkt->data.frame.pts);
      result.size = pkt->data.frame.sz;
      result.data.reset(new uint8_t[result.size]);
      memcpy(result.data.get(), pkt->data.frame.buf, result.size);
      output_cb_.Run(std::move(result), {});
    }
  }
}

}  // namespace media
