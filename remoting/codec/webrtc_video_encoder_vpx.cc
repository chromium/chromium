// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/webrtc_video_encoder_vpx.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/base/cpu_utils.h"
#include "remoting/base/util.h"
#include "remoting/codec/utils.h"
#include "remoting/proto/video.pb.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

// Number of bytes in an RGBx pixel.
constexpr int kBytesPerRgbPixel = 4;

// Magic encoder profile numbers for I420 and I444 input formats.
constexpr int kVp9I420ProfileNumber = 0;
constexpr int kVp9I444ProfileNumber = 1;

// Magic encoder constant for adaptive quantization strategy.
constexpr int kVp9AqModeCyclicRefresh = 3;

constexpr int kDefaultTargetBitrateKbps = 1000;

// Minimum target bitrate per megapixel. The value is chosen experimentally such
// that when screen is not changing the codec converges to the target quantizer
// above in less than 10 frames.
// TODO(zijiehe): This value is for VP8 only; reconsider the value for VP9.
constexpr int kVp8MinimumTargetBitrateKbpsPerMegapixel = 2500;

// Default values for the encoder speed of the supported codecs.
constexpr int kVp9LosslessEncodeSpeed = 5;
constexpr int kVp9DefaultEncoderSpeed = 6;
constexpr int kVp9MaxEncoderSpeed = 9;

void SetCommonCodecParameters(vpx_codec_enc_cfg_t* config,
                              const webrtc::DesktopSize& size) {
  // Use millisecond granularity time base.
  config->g_timebase.num = 1;
  config->g_timebase.den = base::Time::kMillisecondsPerSecond;

  config->g_w = size.width();
  config->g_h = size.height();
  config->g_pass = VPX_RC_ONE_PASS;
  // TODO(joedow): Determine whether it is possible to support portrait mode.
  // VP9 only supports breaking an image up into columns for parallel encoding,
  // this means that a display in portrait mode cannot be broken up into as many
  // columns as a display in landscape mode can so performance will be degraded.
  config->g_threads = WebrtcVideoEncoder::GetEncoderThreadCount(config->g_w);

  // Start emitting packets immediately.
  config->g_lag_in_frames = 0;

  // Since the transport layer is reliable, keyframes should not be necessary.
  // However, due to crbug.com/440223, decoding fails after 30,000 non-key
  // frames, so take the hit of an "unnecessary" key-frame every 10,000 frames.
  config->kf_min_dist = 10000;
  config->kf_max_dist = 10000;

  // Do not drop any frames at encoder.
  config->rc_dropframe_thresh = 0;
  // We do not want variations in bandwidth.
  config->rc_end_usage = VPX_CBR;
  config->rc_undershoot_pct = 100;
  config->rc_overshoot_pct = 15;
}

void SetVp8CodecParameters(vpx_codec_enc_cfg_t* config,
                           const webrtc::DesktopSize& size) {
  SetCommonCodecParameters(config, size);

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Linux, using too many threads for VP8 encoding has been linked to high
  // CPU usage on machines that are under stress. See http://crbug.com/1151148.
  // 5/3/2022 update: Perf testing has shown that doubling the number of threads
  // on machines with a large number of cores will improve performance at higher
  // desktop resolutions.  Doubling the number of threads leads to ~30% increase
  // in framerate at 4K. This could be increased further however we don't want
  // to risk reintroducing the problem from the bug above so take the safe gains
  // and leave plenty of cores for the non-remoting workload.
  uint threshold = config->g_threads >= 16 ? 4U : 2U;
  config->g_threads = std::min(config->g_threads, threshold);
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)

  // Value of 2 means using the real time profile. This is basically a
  // redundant option since we explicitly select real time mode when doing
  // encoding.
  config->g_profile = 2;
}

void SetVp9CodecParameters(vpx_codec_enc_cfg_t* config,
                           const webrtc::DesktopSize& size,
                           bool lossless_color) {
  SetCommonCodecParameters(config, size);

  // Configure VP9 for I420 or I444 source frames.
  config->g_profile =
      lossless_color ? kVp9I444ProfileNumber : kVp9I420ProfileNumber;

  config->rc_end_usage = VPX_CBR;
  // In the absence of a good bandwidth estimator set the target bitrate to a
  // conservative default.
  config->rc_target_bitrate = 500;
}

void SetVp8CodecOptions(vpx_codec_ctx_t* codec) {
  // CPUUSED of 16 will have the smallest CPU load. This turns off sub-pixel
  // motion search.
  vpx_codec_err_t ret = vpx_codec_control(codec, VP8E_SET_CPUUSED, 16);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set CPUUSED";

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  ret = vpx_codec_control(codec, VP8E_SET_NOISE_SENSITIVITY, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set noise sensitivity";
}

void SetVp9CodecOptions(vpx_codec_ctx_t* codec, int encoder_speed) {
  // Note that this knob uses the same parameter name as VP8.
  vpx_codec_err_t ret =
      vpx_codec_control(codec, VP8E_SET_CPUUSED, encoder_speed);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set CPUUSED";

  // Turn on row-based multi-threading if more than one thread is available.
  if (codec->config.enc->g_threads > 1) {
    vpx_codec_control(codec, VP9E_SET_ROW_MT, 1);
  }

  // The param for this knob is a log2 value so 0 is reasonable here.
  vpx_codec_control(codec, VP9E_SET_TILE_COLUMNS,
                    static_cast<int>(std::log2(codec->config.enc->g_threads)));

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  ret = vpx_codec_control(codec, VP9E_SET_NOISE_SENSITIVITY, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set noise sensitivity";

  // Configure the codec to tune it for screen media.
  ret = vpx_codec_control(codec, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set screen content mode";

  // Set cyclic refresh (aka "top-off") for lossy encoding.
  ret = vpx_codec_control(codec, VP9E_SET_AQ_MODE, kVp9AqModeCyclicRefresh);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set aq mode";
}

}  // namespace

// static
std::unique_ptr<WebrtcVideoEncoder> WebrtcVideoEncoderVpx::CreateForVP8() {
  LOG(WARNING) << "VP8 video encoder is created.";
  return base::WrapUnique(new WebrtcVideoEncoderVpx(false));
}

// static
std::unique_ptr<WebrtcVideoEncoder> WebrtcVideoEncoderVpx::CreateForVP9() {
  LOG(WARNING) << "VP9 video encoder is created.";
  return base::WrapUnique(new WebrtcVideoEncoderVpx(true));
}

WebrtcVideoEncoderVpx::~WebrtcVideoEncoderVpx() = default;

void WebrtcVideoEncoderVpx::SetTickClockForTests(
    const base::TickClock* tick_clock) {
  clock_ = tick_clock;
}

void WebrtcVideoEncoderVpx::SetLosslessColor(bool want_lossless) {
  if (!use_vp9_) {
    return;
  }

  if (want_lossless != lossless_color_) {
    lossless_color_ = want_lossless;
    // TODO(wez): Switch to ConfigureCodec() path once libvpx supports it.
    // See https://code.google.com/p/webm/issues/detail?id=913.
    // if (codec_)
    //  Configure(webrtc::DesktopSize(codec_->config.enc->g_w,
    //                                codec_->config.enc->g_h));
    codec_.reset();
  }
}

void WebrtcVideoEncoderVpx::SetEncoderSpeed(int encoder_speed) {
  if (!use_vp9_) {
    return;
  }

  int clamped_speed = std::clamp<int>(encoder_speed, kVp9LosslessEncodeSpeed,
                                      kVp9MaxEncoderSpeed);
  if (vp9_encoder_speed_ != clamped_speed) {
    VLOG(0) << "Setting VP9 encoder speed to " << clamped_speed;
    vp9_encoder_speed_ = clamped_speed;
    codec_.reset();
  }
}

void WebrtcVideoEncoderVpx::Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                                   const FrameParams& params,
                                   EncodeCallback done) {
  // TODO(zijiehe): Replace "if (frame)" with "DCHECK(frame)".
  if (frame) {
    bitrate_filter_.SetFrameSize(frame->size().width(), frame->size().height());
  }

  webrtc::DesktopSize previous_frame_size =
      image_ ? webrtc::DesktopSize(image_->d_w, image_->d_h)
             : webrtc::DesktopSize();

  webrtc::DesktopSize frame_size = frame ? frame->size() : previous_frame_size;

  // Don't need to send anything until we get the first non-null frame.
  if (frame_size.is_empty()) {
    std::move(done).Run(EncodeResult::SUCCEEDED, nullptr);
    return;
  }

  DCHECK_GE(frame_size.width(), 32);
  DCHECK_GE(frame_size.height(), 32);

  // Create or reconfigure the codec to match the size of |frame|.
  if (!codec_ || !frame_size.equals(previous_frame_size)) {
    Configure(frame_size);
  }

  UpdateConfig(params);

  webrtc::DesktopRegion updated_region;
  // Convert the updated capture data ready for encode.
  PrepareImage(frame.get(), &updated_region);

  if (use_active_map_) {
    if (params.clear_active_map || params.key_frame) {
      active_map_data_.Clear();
    }

    // VPX codecs do not use an active map for keyframes so skip in that case.
    if (!params.key_frame) {
      active_map_data_.Update(updated_region);
      if (vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_)) {
        LOG(ERROR) << "Unable to apply active map";
      }
    }
  }

  vpx_codec_err_t ret = vpx_codec_encode(
      codec_.get(), image_.get(), 0, params.duration.InMicroseconds(),
      (params.key_frame) ? VPX_EFLAG_FORCE_KF : 0, VPX_DL_REALTIME);
  if (ret != VPX_CODEC_OK) {
    const char* error_detail = vpx_codec_error_detail(codec_.get());
    LOG(ERROR) << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n  "
               << "Details: " << vpx_codec_error(codec_.get()) << "\n    "
               << (error_detail ? error_detail : "No error details");
    // TODO(zijiehe): A more exact error type is preferred.
    std::move(done).Run(EncodeResult::UNKNOWN_ERROR, nullptr);
    return;
  }

  // Read the encoded data.
  vpx_codec_iter_t iter = nullptr;
  bool got_data = false;

  auto encoded_frame = std::make_unique<EncodedFrame>();
  encoded_frame->dimensions = frame_size;
  if (use_vp9_) {
    encoded_frame->codec = webrtc::kVideoCodecVP9;
  } else {
    encoded_frame->codec = webrtc::kVideoCodecVP8;
  }
  encoded_frame->profile = config_.g_profile;
  if (params.key_frame) {
    encoded_frame->encoded_rect_width = frame_size.width();
    encoded_frame->encoded_rect_height = frame_size.height();
  } else {
    const webrtc::DesktopRect bounding_rectangle =
        GetBoundingRect(updated_region);
    encoded_frame->encoded_rect_width = bounding_rectangle.width();
    encoded_frame->encoded_rect_height = bounding_rectangle.height();
  }

  while (!got_data) {
    const vpx_codec_cx_pkt_t* vpx_packet =
        vpx_codec_get_cx_data(codec_.get(), &iter);
    if (!vpx_packet) {
      continue;
    }

    switch (vpx_packet->kind) {
      case VPX_CODEC_CX_FRAME_PKT: {
        got_data = true;
        encoded_frame->data = webrtc::EncodedImageBuffer::Create(
            reinterpret_cast<const uint8_t*>(vpx_packet->data.frame.buf),
            vpx_packet->data.frame.sz);
        encoded_frame->key_frame =
            vpx_packet->data.frame.flags & VPX_FRAME_IS_KEY;
        CHECK_EQ(vpx_codec_control(codec_.get(), VP8E_GET_LAST_QUANTIZER_64,
                                   &(encoded_frame->quantizer)),
                 VPX_CODEC_OK);
        break;
      }
      default:
        break;
    }
  }

  std::move(done).Run(EncodeResult::SUCCEEDED, std::move(encoded_frame));
}

WebrtcVideoEncoderVpx::WebrtcVideoEncoderVpx(bool use_vp9)
    : use_vp9_(use_vp9),
      image_(nullptr, vpx_img_free),
      clock_(base::DefaultTickClock::GetInstance()),
      bitrate_filter_(kVp8MinimumTargetBitrateKbpsPerMegapixel) {
  // Indicates config is still uninitialized.
  config_.g_timebase.den = 0;

  if (use_vp9_) {
    SetEncoderSpeed(kVp9DefaultEncoderSpeed);
  }
}

void WebrtcVideoEncoderVpx::Configure(const webrtc::DesktopSize& size) {
  DCHECK(use_vp9_ || !lossless_color_);

  if (use_vp9_) {
    VLOG(0) << "Configuring VP9 encoder with lossless-color="
            << (lossless_color_ ? "true" : "false") << ".";
  }

  // Tear down |image_| if it doesn't match the new frame size.
  if (image_ && !size.equals(webrtc::DesktopSize(image_->d_w, image_->d_h))) {
    image_.reset();
  }

  // Tear down |codec_| if the new size does not match what this codec instance
  // was configured for.
  if (codec_ && !size.equals(webrtc::DesktopSize(codec_->config.enc->g_w,
                                                 codec_->config.enc->g_h))) {
    codec_.reset();
  }

  if (use_active_map_) {
    active_map_data_.Initialize(size);
    active_map_ = {
        .active_map = active_map_data_.data(),
        .rows = active_map_data_.height(),
        .cols = active_map_data_.width(),
    };
  }

  // Fetch a default configuration for the desired codec.
  const vpx_codec_iface_t* interface =
      use_vp9_ ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
  vpx_codec_err_t ret = vpx_codec_enc_config_default(interface, &config_, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to fetch default configuration";

  // Customize the default configuration to our needs.
  if (use_vp9_) {
    SetVp9CodecParameters(&config_, size, lossless_color_);
  } else {
    SetVp8CodecParameters(&config_, size);
  }

  config_.rc_target_bitrate = kDefaultTargetBitrateKbps;

  // Initialize or re-configure the codec with the custom configuration.
  if (!codec_) {
    codec_.reset(new vpx_codec_ctx_t);
    ret = vpx_codec_enc_init(codec_.get(), interface, &config_, 0);
    CHECK_EQ(VPX_CODEC_OK, ret) << "Failed to initialize codec";
  } else {
    ret = vpx_codec_enc_config_set(codec_.get(), &config_);
    CHECK_EQ(VPX_CODEC_OK, ret) << "Failed to reconfigure codec";
  }

  // Apply further customizations to the codec now it's initialized.
  if (use_vp9_) {
    SetVp9CodecOptions(codec_.get(), vp9_encoder_speed_);
  } else {
    SetVp8CodecOptions(codec_.get());
  }
}

void WebrtcVideoEncoderVpx::UpdateConfig(const FrameParams& params) {
  // Configuration not initialized.
  if (config_.g_timebase.den == 0) {
    return;
  }

  bool changed = false;

  if (params.bitrate_kbps >= 0) {
    bitrate_filter_.SetBandwidthEstimateKbps(params.bitrate_kbps);
    if (config_.rc_target_bitrate !=
        static_cast<unsigned int>(bitrate_filter_.GetTargetBitrateKbps())) {
      config_.rc_target_bitrate = bitrate_filter_.GetTargetBitrateKbps();
      changed = true;
    }
  }

  if (params.vpx_min_quantizer >= 0 &&
      config_.rc_min_quantizer !=
          static_cast<unsigned int>(params.vpx_min_quantizer)) {
    config_.rc_min_quantizer = params.vpx_min_quantizer;
    changed = true;
  }

  if (params.vpx_max_quantizer >= 0 &&
      config_.rc_max_quantizer !=
          static_cast<unsigned int>(params.vpx_max_quantizer)) {
    config_.rc_max_quantizer = params.vpx_max_quantizer;
    changed = true;
  }

  if (!changed) {
    return;
  }

  // Update encoder context.
  if (vpx_codec_enc_config_set(codec_.get(), &config_)) {
    NOTREACHED() << "Unable to set encoder config";
  }
}

void WebrtcVideoEncoderVpx::PrepareImage(
    const webrtc::DesktopFrame* frame,
    webrtc::DesktopRegion* updated_region) {
  updated_region->Clear();

  if (!frame) {
    return;
  }

  if (image_) {
    // Pad each rectangle to avoid the block-artifact filters in libvpx from
    // introducing artifacts; VP9 includes up to 8px either side, and VP8 up to
    // 3px, so unchanged pixels up to that far out may still be affected by the
    // changes in the updated region, and so must be listed in the active map.
    // After padding we align each rectangle to 16x16 active-map macroblocks.
    // This implicitly ensures all rects have even top-left coords, which is
    // is required by ConvertRGBToYUVWithRect().
    // TODO(wez): Do we still need 16x16 align, or is even alignment sufficient?
    int padding = use_vp9_ ? 8 : 3;
    for (webrtc::DesktopRegion::Iterator r(frame->updated_region());
         !r.IsAtEnd(); r.Advance()) {
      const webrtc::DesktopRect& rect = r.rect();
      updated_region->AddRect(AlignRect(webrtc::DesktopRect::MakeLTRB(
          rect.left() - padding, rect.top() - padding, rect.right() + padding,
          rect.bottom() + padding)));
    }

    // Clip back to the screen dimensions, in case they're not macroblock
    // aligned. The conversion routines don't require even width & height,
    // so this is safe even if the source dimensions are not even.
    updated_region->IntersectWith(
        webrtc::DesktopRect::MakeWH(image_->d_w, image_->d_h));
  } else {
    vpx_img_fmt_t fmt = lossless_color_ ? VPX_IMG_FMT_I444 : VPX_IMG_FMT_I420;
    image_.reset(vpx_img_alloc(nullptr, fmt, frame->size().width(),
                               frame->size().height(),
                               GetSimdMemoryAlignment()));
    updated_region->AddRect(
        webrtc::DesktopRect::MakeWH(image_->d_w, image_->d_h));
  }

  // Convert the updated region to YUV ready for encoding.
  const uint8_t* rgb_data = frame->data();
  const int rgb_stride = frame->stride();
  const int y_stride = image_->stride[0];
  DCHECK_EQ(image_->stride[1], image_->stride[2]);
  const int uv_stride = image_->stride[1];
  uint8_t* y_data = image_->planes[0];
  uint8_t* u_data = image_->planes[1];
  uint8_t* v_data = image_->planes[2];

  switch (image_->fmt) {
    case VPX_IMG_FMT_I444:
      for (webrtc::DesktopRegion::Iterator r(*updated_region); !r.IsAtEnd();
           r.Advance()) {
        webrtc::DesktopRect rect = GetRowAlignedRect(r.rect(), image_->d_w);
        int rgb_offset =
            rgb_stride * rect.top() + rect.left() * kBytesPerRgbPixel;
        int yuv_offset = uv_stride * rect.top() + rect.left();
        libyuv::ARGBToI444(rgb_data + rgb_offset, rgb_stride,
                           y_data + yuv_offset, y_stride, u_data + yuv_offset,
                           uv_stride, v_data + yuv_offset, uv_stride,
                           rect.width(), rect.height());
      }
      break;
    case VPX_IMG_FMT_I420:
      for (webrtc::DesktopRegion::Iterator r(*updated_region); !r.IsAtEnd();
           r.Advance()) {
        webrtc::DesktopRect rect = GetRowAlignedRect(r.rect(), image_->d_w);
        int rgb_offset =
            rgb_stride * rect.top() + rect.left() * kBytesPerRgbPixel;
        int y_offset = y_stride * rect.top() + rect.left();
        int uv_offset = uv_stride * rect.top() / 2 + rect.left() / 2;
        libyuv::ARGBToI420(rgb_data + rgb_offset, rgb_stride, y_data + y_offset,
                           y_stride, u_data + uv_offset, uv_stride,
                           v_data + uv_offset, uv_stride, rect.width(),
                           rect.height());
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace remoting
