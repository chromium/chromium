// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/vpx_encoder.h"

#include <algorithm>

#include "base/system/sys_info.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrame;
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

// static
void VpxEncoder::ShutdownEncoder(std::unique_ptr<Thread> encoding_thread,
                                 ScopedVpxCodecCtxPtr encoder) {
  DCHECK(encoding_thread);
  // Both |encoding_thread| and |encoder| will be destroyed at end-of-scope.
}

VpxEncoder::VpxEncoder(
    bool use_vp9,
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : VideoTrackRecorder::Encoder(on_encoded_video_callback,
                                  bits_per_second,
                                  std::move(main_task_runner)),
      use_vp9_(use_vp9) {
  codec_config_.g_timebase.den = 0;        // Not initialized.
  alpha_codec_config_.g_timebase.den = 0;  // Not initialized.
  DCHECK(encoding_thread_);
}

VpxEncoder::~VpxEncoder() {
  PostCrossThreadTask(
      *main_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&VpxEncoder::ShutdownEncoder,
                          std::move(encoding_thread_), std::move(encoder_)));
}

bool VpxEncoder::CanEncodeAlphaChannel() {
  return true;
}

void VpxEncoder::EncodeOnEncodingTaskRunner(scoped_refptr<VideoFrame> frame,
                                            base::TimeTicks capture_timestamp) {
  TRACE_EVENT0("media", "VpxEncoder::EncodeOnEncodingTaskRunner");
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  if (frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER)
    frame = ConvertToI420ForSoftwareEncoder(frame);

  const gfx::Size frame_size = frame->visible_rect().size();
  base::TimeDelta duration = EstimateFrameDuration(*frame);
  const media::WebmMuxer::VideoParameters video_params(frame);

  if (!IsInitialized(codec_config_) ||
      gfx::Size(codec_config_.g_w, codec_config_.g_h) != frame_size) {
    ConfigureEncoderOnEncodingTaskRunner(frame_size, &codec_config_, &encoder_);
  }

  const bool frame_has_alpha = frame->format() == media::PIXEL_FORMAT_I420A;
  // Split the duration between two encoder instances if alpha is encoded.
  duration = frame_has_alpha ? duration / 2 : duration;
  if (frame_has_alpha && (!IsInitialized(alpha_codec_config_) ||
                          gfx::Size(alpha_codec_config_.g_w,
                                    alpha_codec_config_.g_h) != frame_size)) {
    ConfigureEncoderOnEncodingTaskRunner(frame_size, &alpha_codec_config_,
                                         &alpha_encoder_);
    u_plane_stride_ = media::VideoFrame::RowBytes(
        VideoFrame::kUPlane, frame->format(), frame_size.width());
    v_plane_stride_ = media::VideoFrame::RowBytes(
        VideoFrame::kVPlane, frame->format(), frame_size.width());
    v_plane_offset_ = media::VideoFrame::PlaneSize(
                          frame->format(), VideoFrame::kUPlane, frame_size)
                          .GetArea();
    alpha_dummy_planes_.resize(SafeCast<wtf_size_t>(
        v_plane_offset_ + media::VideoFrame::PlaneSize(
                              frame->format(), VideoFrame::kVPlane, frame_size)
                              .GetArea()));
    // It is more expensive to encode 0x00, so use 0x80 instead.
    std::fill(alpha_dummy_planes_.begin(), alpha_dummy_planes_.end(), 0x80);
  }
  // If we introduced a new alpha frame, force keyframe.
  const bool force_keyframe = frame_has_alpha && !last_frame_had_alpha_;
  last_frame_had_alpha_ = frame_has_alpha;

  std::string data;
  bool keyframe = false;
  DoEncode(encoder_.get(), frame_size, frame->data(VideoFrame::kYPlane),
           frame->visible_data(VideoFrame::kYPlane),
           frame->stride(VideoFrame::kYPlane),
           frame->visible_data(VideoFrame::kUPlane),
           frame->stride(VideoFrame::kUPlane),
           frame->visible_data(VideoFrame::kVPlane),
           frame->stride(VideoFrame::kVPlane), duration, force_keyframe, data,
           &keyframe);

  std::string alpha_data;
  if (frame_has_alpha) {
    bool alpha_keyframe = false;
    DoEncode(alpha_encoder_.get(), frame_size, frame->data(VideoFrame::kAPlane),
             frame->visible_data(VideoFrame::kAPlane),
             frame->stride(VideoFrame::kAPlane), alpha_dummy_planes_.data(),
             SafeCast<int>(u_plane_stride_),
             alpha_dummy_planes_.data() + v_plane_offset_,
             SafeCast<int>(v_plane_stride_), duration, keyframe, alpha_data,
             &alpha_keyframe);
    DCHECK_EQ(keyframe, alpha_keyframe);
  }
  frame = nullptr;

  PostCrossThreadTask(
      *origin_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(
          OnFrameEncodeCompleted,
          WTF::Passed(CrossThreadBindRepeating(on_encoded_video_callback_)),
          video_params, std::move(data), std::move(alpha_data),
          capture_timestamp, keyframe));
}

void VpxEncoder::DoEncode(vpx_codec_ctx_t* const encoder,
                          const gfx::Size& frame_size,
                          uint8_t* const data,
                          uint8_t* const y_plane,
                          int y_stride,
                          uint8_t* const u_plane,
                          int u_stride,
                          uint8_t* const v_plane,
                          int v_stride,
                          const base::TimeDelta& duration,
                          bool force_keyframe,
                          std::string& output_data,
                          bool* const keyframe) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  vpx_image_t vpx_image;
  vpx_image_t* const result =
      vpx_img_wrap(&vpx_image, VPX_IMG_FMT_I420, frame_size.width(),
                   frame_size.height(), 1 /* align */, data);
  DCHECK_EQ(result, &vpx_image);
  vpx_image.planes[VPX_PLANE_Y] = y_plane;
  vpx_image.planes[VPX_PLANE_U] = u_plane;
  vpx_image.planes[VPX_PLANE_V] = v_plane;
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
  DCHECK_EQ(ret, VPX_CODEC_OK)
      << vpx_codec_err_to_string(ret) << ", #" << vpx_codec_error(encoder)
      << " -" << vpx_codec_error_detail(encoder);

  *keyframe = false;
  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* pkt = nullptr;
  while ((pkt = vpx_codec_get_cx_data(encoder, &iter))) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    output_data.assign(static_cast<char*>(pkt->data.frame.buf),
                       pkt->data.frame.sz);
    *keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
    break;
  }
}

void VpxEncoder::ConfigureEncoderOnEncodingTaskRunner(
    const gfx::Size& size,
    vpx_codec_enc_cfg_t* codec_config,
    ScopedVpxCodecCtxPtr* encoder) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
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

  // Let the encoder decide where to place the Keyframes, between min and max.
  // In VPX_KF_AUTO mode libvpx will sometimes emit keyframes regardless of min/
  // max distance out of necessity.
  // Note that due to http://crbug.com/440223, it might be necessary to force a
  // key frame after 10,000frames since decoding fails after 30,000 non-key
  // frames.
  // Forcing a keyframe in regular intervals also allows seeking in the
  // resulting recording with decent performance.
  codec_config->kf_mode = VPX_KF_AUTO;
  codec_config->kf_min_dist = 0;
  codec_config->kf_max_dist = 100;

  codec_config->g_threads = GetNumberOfThreadsForEncoding();

  // Number of frames to consume before producing output.
  codec_config->g_lag_in_frames = 0;

  encoder->reset(new vpx_codec_ctx_t);
  const vpx_codec_err_t ret = vpx_codec_enc_init(
      encoder->get(), codec_interface, codec_config, 0 /* flags */);
  DCHECK_EQ(VPX_CODEC_OK, ret);

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
}

bool VpxEncoder::IsInitialized(const vpx_codec_enc_cfg_t& codec_config) const {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());
  return codec_config.g_timebase.den != 0;
}

base::TimeDelta VpxEncoder::EstimateFrameDuration(const VideoFrame& frame) {
  DCHECK(encoding_task_runner_->BelongsToCurrentThread());

  using base::TimeDelta;
  base::TimeDelta predicted_frame_duration;
  if (!frame.metadata()->GetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                      &predicted_frame_duration) ||
      predicted_frame_duration <= base::TimeDelta()) {
    // The source of the video frame did not provide the frame duration.  Use
    // the actual amount of time between the current and previous frame as a
    // prediction for the next frame's duration.
    // TODO(mcasas): This duration estimation could lead to artifacts if the
    // cadence of the received stream is compromised (e.g. camera freeze, pause,
    // remote packet loss).  Investigate using GetFrameRate() in this case.
    predicted_frame_duration = frame.timestamp() - last_frame_timestamp_;
  }
  last_frame_timestamp_ = frame.timestamp();
  // Make sure |predicted_frame_duration| is in a safe range of values.
  const base::TimeDelta kMaxFrameDuration =
      base::TimeDelta::FromSecondsD(1.0 / 8);
  const base::TimeDelta kMinFrameDuration =
      base::TimeDelta::FromMilliseconds(1);
  return std::min(kMaxFrameDuration,
                  std::max(predicted_frame_duration, kMinFrameDuration));
}

}  // namespace blink
