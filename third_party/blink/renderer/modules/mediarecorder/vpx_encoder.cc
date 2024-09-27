// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediarecorder/vpx_encoder.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrameMetadata;

namespace blink {

void VpxEncoder::VpxCodecDeleter::operator()(vpx_codec_ctx_t* codec) {
  if (!codec)
    return;
  vpx_codec_err_t ret = vpx_codec_destroy(codec);
  CHECK_EQ(ret, VPX_CODEC_OK);
  delete codec;
}

static int GetNumberOfThreadsForEncoding() {
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  return std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);
}

VpxEncoder::VpxEncoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    bool use_vp9,
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    uint32_t bits_per_second,
    bool is_screencast,
    const VideoTrackRecorder::OnErrorCB on_error_cb)
    : Encoder(std::move(encoding_task_runner),
              on_encoded_video_cb,
              bits_per_second),
      use_vp9_(use_vp9),
      is_screencast_(is_screencast),
      on_error_cb_(on_error_cb) {
  std::memset(&codec_config_, 0, sizeof(codec_config_));
  std::memset(&alpha_codec_config_, 0, sizeof(alpha_codec_config_));
  codec_config_.g_timebase.den = 0;        // Not initialized.
  alpha_codec_config_.g_timebase.den = 0;  // Not initialized.
}

bool VpxEncoder::CanEncodeAlphaChannel() const {
  return true;
}

void VpxEncoder::EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                             base::TimeTicks capture_timestamp,
                             bool request_keyframe) {
  using media::VideoFrame;
  TRACE_EVENT0("media", "VpxEncoder::EncodeFrame");

  if (frame->format() == media::PIXEL_FORMAT_NV12 &&
      frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER)
    frame = media::ConvertToMemoryMappedFrame(frame);
  if (!frame) {
    LOG(WARNING) << "Invalid video frame to encode";
    return;
  }

  const gfx::Size frame_size = frame->visible_rect().size();
  base::TimeDelta duration = EstimateFrameDuration(*frame);
  const media::Muxer::VideoParameters video_params(*frame);

  if (!IsInitialized(codec_config_) ||
      gfx::Size(codec_config_.g_w, codec_config_.g_h) != frame_size) {
    if (!ConfigureEncoder(frame_size, &codec_config_, &encoder_)) {
      return;
    }
  }

  bool keyframe = false;
  bool force_keyframe = request_keyframe;
  bool alpha_keyframe = false;
  scoped_refptr<media::DecoderBuffer> output_data;
  switch (frame->format()) {
    case media::PIXEL_FORMAT_NV12: {
      last_frame_had_alpha_ = false;
      DoEncode(encoder_.get(), frame_size, frame->data(VideoFrame::Plane::kY),
               frame->visible_data(VideoFrame::Plane::kY),
               frame->stride(VideoFrame::Plane::kY),
               frame->visible_data(VideoFrame::Plane::kUV),
               frame->stride(VideoFrame::Plane::kUV),
               frame->visible_data(VideoFrame::Plane::kUV) + 1,
               frame->stride(VideoFrame::Plane::kUV), duration, force_keyframe,
               &output_data, /*is_alpha=*/false, VPX_IMG_FMT_NV12);
      break;
    }
    case media::PIXEL_FORMAT_I420: {
      last_frame_had_alpha_ = false;
      DoEncode(encoder_.get(), frame_size, frame->data(VideoFrame::Plane::kY),
               frame->visible_data(VideoFrame::Plane::kY),
               frame->stride(VideoFrame::Plane::kY),
               frame->visible_data(VideoFrame::Plane::kU),
               frame->stride(VideoFrame::Plane::kU),
               frame->visible_data(VideoFrame::Plane::kV),
               frame->stride(VideoFrame::Plane::kV), duration, force_keyframe,
               &output_data, /*is_alpha=*/false, VPX_IMG_FMT_I420);
      break;
    }
    case media::PIXEL_FORMAT_I420A: {
      // Split the duration between two encoder instances if alpha is encoded.
      duration = duration / 2;
      if ((!IsInitialized(alpha_codec_config_) ||
           gfx::Size(alpha_codec_config_.g_w, alpha_codec_config_.g_h) !=
               frame_size)) {
        if (!ConfigureEncoder(frame_size, &alpha_codec_config_,
                              &alpha_encoder_)) {
          return;
        }
        u_plane_stride_ = media::VideoFrame::RowBytes(
            VideoFrame::Plane::kU, frame->format(), frame_size.width());
        v_plane_stride_ = media::VideoFrame::RowBytes(
            VideoFrame::Plane::kV, frame->format(), frame_size.width());
        v_plane_offset_ =
            media::VideoFrame::PlaneSize(frame->format(), VideoFrame::Plane::kU,
                                         frame_size)
                .GetArea();
        alpha_dummy_planes_.resize(base::checked_cast<wtf_size_t>(
            v_plane_offset_ +
            media::VideoFrame::PlaneSize(frame->format(), VideoFrame::Plane::kV,
                                         frame_size)
                .GetArea()));
        // It is more expensive to encode 0x00, so use 0x80 instead.
        std::fill(alpha_dummy_planes_.begin(), alpha_dummy_planes_.end(), 0x80);
      }
      // If we introduced a new alpha frame, force keyframe.
      force_keyframe = force_keyframe || !last_frame_had_alpha_;
      last_frame_had_alpha_ = true;

      DoEncode(encoder_.get(), frame_size, frame->data(VideoFrame::Plane::kY),
               frame->visible_data(VideoFrame::Plane::kY),
               frame->stride(VideoFrame::Plane::kY),
               frame->visible_data(VideoFrame::Plane::kU),
               frame->stride(VideoFrame::Plane::kU),
               frame->visible_data(VideoFrame::Plane::kV),
               frame->stride(VideoFrame::Plane::kV), duration, force_keyframe,
               &output_data, /*is_alpha=*/false, VPX_IMG_FMT_I420);

      DoEncode(alpha_encoder_.get(), frame_size,
               frame->data(VideoFrame::Plane::kA),
               frame->visible_data(VideoFrame::Plane::kA),
               frame->stride(VideoFrame::Plane::kA), alpha_dummy_planes_.data(),
               base::checked_cast<int>(u_plane_stride_),
               alpha_dummy_planes_.data() + v_plane_offset_,
               base::checked_cast<int>(v_plane_stride_), duration, keyframe,
               &output_data, /*is_alpha=*/true, VPX_IMG_FMT_I420);
      DCHECK_EQ(keyframe, alpha_keyframe);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION()
          << media::VideoPixelFormatToString(frame->format());
  }
  frame = nullptr;

  metrics_provider_->IncrementEncodedFrameCount();
  on_encoded_video_cb_.Run(video_params, std::move(output_data), std::nullopt,
                           capture_timestamp);
}

void VpxEncoder::DoEncode(vpx_codec_ctx_t* const encoder,
                          const gfx::Size& frame_size,
                          const uint8_t* data,
                          const uint8_t* y_plane,
                          int y_stride,
                          const uint8_t* u_plane,
                          int u_stride,
                          const uint8_t* v_plane,
                          int v_stride,
                          const base::TimeDelta& duration,
                          bool force_keyframe,
                          scoped_refptr<media::DecoderBuffer>* output_data,
                          bool is_alpha,
                          vpx_img_fmt_t img_fmt) {
  CHECK(output_data);
  DCHECK(img_fmt == VPX_IMG_FMT_I420 || img_fmt == VPX_IMG_FMT_NV12);

  vpx_image_t vpx_image;
  vpx_image_t* const result =
      vpx_img_wrap(&vpx_image, img_fmt, frame_size.width(), frame_size.height(),
                   1 /* align */, const_cast<uint8_t*>(data));
  DCHECK_EQ(result, &vpx_image);
  vpx_image.planes[VPX_PLANE_Y] = const_cast<uint8_t*>(y_plane);
  vpx_image.planes[VPX_PLANE_U] = const_cast<uint8_t*>(u_plane);
  vpx_image.planes[VPX_PLANE_V] = const_cast<uint8_t*>(v_plane);
  vpx_image.stride[VPX_PLANE_Y] = y_stride;
  vpx_image.stride[VPX_PLANE_U] = u_stride;
  vpx_image.stride[VPX_PLANE_V] = v_stride;

  const vpx_codec_flags_t flags = force_keyframe ? VPX_EFLAG_FORCE_KF : 0;
  // Encode the frame.  The presentation time stamp argument here is fixed to
  // zero to force the encoder to base its single-frame bandwidth calculations
  // entirely on |predicted_frame_duration|.
  const vpx_codec_err_t ret =
      vpx_codec_encode(encoder, &vpx_image, 0 /* pts */,
                       static_cast<unsigned long>(duration.InMicroseconds()),
                       flags, VPX_DL_REALTIME);
  if (ret != VPX_CODEC_OK) {
    metrics_provider_->SetError(
        {media::EncoderStatus::Codes::kEncoderFailedEncode,
         base::StrCat(
             {"libvpx failed to encode: ", vpx_codec_err_to_string(ret), " - ",
              vpx_codec_error_detail(encoder)})});
    on_error_cb_.Run();
    return;
  }

  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  while ((pkt = vpx_codec_get_cx_data(encoder, &iter))) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    if (is_alpha) {
      const auto* alpha_data =
          reinterpret_cast<const uint8_t*>(pkt->data.frame.buf);
      // If is_alpha is true, it needs *output_data to already be a valid
      // scoped_refptr.
      CHECK(*output_data);
      (*output_data)
          ->WritableSideData()
          .alpha_data.assign(alpha_data, alpha_data + pkt->data.frame.sz);
    } else {
      *output_data = media::DecoderBuffer::CopyFrom(
          {reinterpret_cast<const uint8_t*>(pkt->data.frame.buf),
           pkt->data.frame.sz});
    }
    (*output_data)
        ->set_is_key_frame((pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0);
    break;
  }
}

bool VpxEncoder::ConfigureEncoder(const gfx::Size& size,
                                  vpx_codec_enc_cfg_t* codec_config,
                                  ScopedVpxCodecCtxPtr* encoder) {
  if (IsInitialized(*codec_config)) {
    // TODO(mcasas) VP8 quirk/optimisation: If the new |size| is strictly less-
    // than-or-equal than the old size, in terms of area, the existing encoder
    // instance could be reused after changing |codec_config->{g_w,g_h}|.
    DVLOG(1) << "Destroying/Re-Creating encoder for new frame size: "
             << gfx::Size(codec_config->g_w, codec_config->g_h).ToString()
             << " --> " << size.ToString() << (use_vp9_ ? " vp9" : " vp8");
    encoder->reset();
  }

  const vpx_codec_iface_t* codec_interface =
      use_vp9_ ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
  vpx_codec_err_t result = vpx_codec_enc_config_default(
      codec_interface, codec_config, 0 /* reserved */);
  DCHECK_EQ(VPX_CODEC_OK, result);

  DCHECK_EQ(320u, codec_config->g_w);
  DCHECK_EQ(240u, codec_config->g_h);
  DCHECK_EQ(256u, codec_config->rc_target_bitrate);
  // Use the selected bitrate or adjust default bit rate to account for the
  // actual size.  Note: |rc_target_bitrate| units are kbit per second.
  if (bits_per_second_ > 0) {
    codec_config->rc_target_bitrate = bits_per_second_ / 1000;
  } else {
    codec_config->rc_target_bitrate = size.GetArea() *
                                      codec_config->rc_target_bitrate /
                                      codec_config->g_w / codec_config->g_h;
  }
  // Don't drop a frame.
  DCHECK_EQ(codec_config->rc_dropframe_thresh, 0u);
  // Both VP8/VP9 configuration should be Variable BitRate by default.
  DCHECK_EQ(VPX_VBR, codec_config->rc_end_usage);
  if (use_vp9_) {
    // Number of frames to consume before producing output.
    codec_config->g_lag_in_frames = 0;

    // DCHECK that the profile selected by default is I420 (magic number 0).
    DCHECK_EQ(0u, codec_config->g_profile);
  } else {
    // VP8 always produces frames instantaneously.
    DCHECK_EQ(0u, codec_config->g_lag_in_frames);
  }

  DCHECK(size.width());
  DCHECK(size.height());
  codec_config->g_w = size.width();
  codec_config->g_h = size.height();
  codec_config->g_pass = VPX_RC_ONE_PASS;

  // Timebase is the smallest interval used by the stream, can be set to the
  // frame rate or to e.g. microseconds.
  codec_config->g_timebase.num = 1;
  codec_config->g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // The periodical keyframe interval is configured by KeyFrameRequestProcessor.
  // Aside from the periodical keyframe, let the encoder decide where to place
  // the Keyframes In VPX_KF_AUTO mode libvpx will sometimes emit keyframes out
  // of necessity.
  // Note that due to http://crbug.com/440223, it might be necessary to force a
  // key frame after 10,000frames since decoding fails after 30,000 non-key
  // frames.
  codec_config->kf_mode = VPX_KF_AUTO;

  codec_config->g_threads = GetNumberOfThreadsForEncoding();

  // Number of frames to consume before producing output.
  codec_config->g_lag_in_frames = 0;

  metrics_provider_->Initialize(
      use_vp9_ ? media::VP9PROFILE_MIN : media::VP8PROFILE_ANY, size,
      /*is_hardware_encoder=*/false);
  // Can't use ScopedVpxCodecCtxPtr until after vpx_codec_enc_init, since it's
  // not valid to call vpx_codec_destroy when vpx_codec_enc_init fails.
  auto tmp_encoder = std::make_unique<vpx_codec_ctx_t>();
  const vpx_codec_err_t ret = vpx_codec_enc_init(
      tmp_encoder.get(), codec_interface, codec_config, 0 /* flags */);
  if (ret != VPX_CODEC_OK) {
    metrics_provider_->SetError(
        {media::EncoderStatus::Codes::kEncoderInitializationError,
         base::StrCat(
             {"libvpx failed to initialize: ", vpx_codec_err_to_string(ret)})});
    DLOG(WARNING) << "vpx_codec_enc_init failed: " << ret;
    // Require the encoder to be reinitialized next frame.
    codec_config->g_timebase.den = 0;
    on_error_cb_.Run();
    return false;
  }
  encoder->reset(tmp_encoder.release());

  if (use_vp9_) {
    // Values of VP8E_SET_CPUUSED greater than 0 will increase encoder speed at
    // the expense of quality up to a maximum value of 8 for VP9, by tuning the
    // target time spent encoding the frame. Go from 8 to 5 (values for real
    // time encoding) depending on the amount of cores available in the system.
    const int kCpuUsed =
        std::max(5, 8 - base::SysInfo::NumberOfProcessors() / 2);
    result = vpx_codec_control(encoder->get(), VP8E_SET_CPUUSED, kCpuUsed);
    DLOG_IF(WARNING, VPX_CODEC_OK != result) << "VP8E_SET_CPUUSED failed";
  }

  // Tune configs for screen sharing. The values are the same as WebRTC
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.cc
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/vp9/libvpx_vp9_encoder.cc
  vpx_codec_control(encoder->get(), VP8E_SET_STATIC_THRESHOLD,
                    (is_screencast_ && !use_vp9_) ? 100 : 1);
  if (is_screencast_) {
    if (use_vp9_) {
      vpx_codec_control(encoder->get(), VP9E_SET_TUNE_CONTENT,
                        VP9E_CONTENT_SCREEN);
    } else {
      // Setting 1, not 2, so the libvpx encoder doesn't drop a frame.
      vpx_codec_control(encoder->get(), VP8E_SET_SCREEN_CONTENT_MODE, 1 /*On*/);
    }
  }

  return true;
}

bool VpxEncoder::IsInitialized(const vpx_codec_enc_cfg_t& codec_config) const {
  return codec_config.g_timebase.den != 0;
}

base::TimeDelta VpxEncoder::EstimateFrameDuration(
    const media::VideoFrame& frame) {
  // If the source of the video frame did not provide the frame duration, use
  // the actual amount of time between the current and previous frame as a
  // prediction for the next frame's duration.
  // TODO(mcasas): This duration estimation could lead to artifacts if the
  // cadence of the received stream is compromised (e.g. camera freeze, pause,
  // remote packet loss).  Investigate using GetFrameRate() in this case.
  base::TimeDelta predicted_frame_duration =
      frame.timestamp() - last_frame_timestamp_;
  base::TimeDelta frame_duration =
      frame.metadata().frame_duration.value_or(predicted_frame_duration);
  last_frame_timestamp_ = frame.timestamp();
  // Make sure |frame_duration| is in a safe range of values.
  const base::TimeDelta kMaxFrameDuration = base::Seconds(1.0 / 8);
  const base::TimeDelta kMinFrameDuration = base::Milliseconds(1);
  return std::min(kMaxFrameDuration,
                  std::max(frame_duration, kMinFrameDuration));
}

}  // namespace blink
