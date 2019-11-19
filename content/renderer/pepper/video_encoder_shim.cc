// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/video_encoder_shim.h"

#include <inttypes.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/pepper/pepper_video_encoder_host.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

// TODO(llandwerlin): Libvpx doesn't seem to have a maximum frame size
// limitation. We currently limit the size of the frames to encode at
// 2160p (%64 pixels blocks), this seems like a reasonable limit for
// software encoding.
const int32_t kMaxWidth = 4096;
const int32_t kMaxHeight = 2176;

// Bitstream buffer size.
const uint32_t kBitstreamBufferSize = 2 * 1024 * 1024;

// Number of frames needs at any given time.
const uint32_t kInputFrameCount = 1;

// Maximal number or threads used for encoding.
const int32_t kMaxNumThreads = 8;

// Default speed for the encoder. Increases the CPU usage as the value
// is more negative (VP8 valid range: -16..16, VP9 valid range:
// -8..8), using the same value as WebRTC.
const int32_t kVp8DefaultCpuUsed = -6;

// Default quantizer min/max values (same values as WebRTC).
const int32_t kVp8DefaultMinQuantizer = 2;
const int32_t kVp8DefaultMaxQuantizer = 52;

// Maximum bitrate in CQ mode (same value as ffmpeg).
const int32_t kVp8MaxCQBitrate = 1000000;

// For VP9, the following 3 values are the same values as remoting.
const int32_t kVp9DefaultCpuUsed = 6;

const int32_t kVp9DefaultMinQuantizer = 20;
const int32_t kVp9DefaultMaxQuantizer = 30;

// VP9 adaptive quantization strategy (same as remoting (live video
// conferencing)).
const int kVp9AqModeCyclicRefresh = 3;

void GetVpxCodecParameters(media::VideoCodecProfile codec,
                           vpx_codec_iface_t** vpx_codec,
                           int32_t* min_quantizer,
                           int32_t* max_quantizer,
                           int32_t* cpu_used) {
  switch (codec) {
    case media::VP8PROFILE_ANY:
      *vpx_codec = vpx_codec_vp8_cx();
      *min_quantizer = kVp8DefaultMinQuantizer;
      *max_quantizer = kVp8DefaultMaxQuantizer;
      *cpu_used = kVp8DefaultCpuUsed;
      break;
    // Only VP9 profile 0 is supported by PPAPI at the moment. VP9 profiles 1-3
    // are not supported due to backward compatibility.
    case media::VP9PROFILE_PROFILE0:
      *vpx_codec = vpx_codec_vp9_cx();
      *min_quantizer = kVp9DefaultMinQuantizer;
      *max_quantizer = kVp9DefaultMaxQuantizer;
      *cpu_used = kVp9DefaultCpuUsed;
      break;
    default:
      *vpx_codec = nullptr;
      *min_quantizer = 0;
      *max_quantizer = 0;
      *cpu_used = 0;
      NOTREACHED();
  }
}

}  // namespace

class VideoEncoderShim::EncoderImpl {
 public:
  explicit EncoderImpl(const base::WeakPtr<VideoEncoderShim>& shim);
  ~EncoderImpl();

  void Initialize(const media::VideoEncodeAccelerator::Config& config);
  void Encode(scoped_refptr<media::VideoFrame> frame, bool force_keyframe);
  void UseOutputBitstreamBuffer(media::BitstreamBuffer buffer, uint8_t* mem);
  void RequestEncodingParametersChange(uint32_t bitrate, uint32_t framerate);
  void Stop();

 private:
  struct PendingEncode {
    PendingEncode(scoped_refptr<media::VideoFrame> frame, bool force_keyframe)
        : frame(std::move(frame)), force_keyframe(force_keyframe) {}
    ~PendingEncode() {}

    scoped_refptr<media::VideoFrame> frame;
    bool force_keyframe;
  };

  struct BitstreamBuffer {
    BitstreamBuffer(media::BitstreamBuffer buffer, uint8_t* mem)
        : buffer(std::move(buffer)), mem(mem) {}
    BitstreamBuffer(BitstreamBuffer&&) = default;
    ~BitstreamBuffer() {}

    media::BitstreamBuffer buffer;
    uint8_t* mem;
  };

  void DoEncode();
  void NotifyError(media::VideoEncodeAccelerator::Error error);

  base::WeakPtr<VideoEncoderShim> shim_;
  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner_;

  bool initialized_;

  // Libvpx internal objects. Only valid if |initialized_| is true.
  vpx_codec_enc_cfg_t config_;
  vpx_codec_ctx_t encoder_;

  uint32_t framerate_;

  base::circular_deque<PendingEncode> frames_;
  base::circular_deque<BitstreamBuffer> buffers_;
};

VideoEncoderShim::EncoderImpl::EncoderImpl(
    const base::WeakPtr<VideoEncoderShim>& shim)
    : shim_(shim),
      renderer_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      initialized_(false) {
}

VideoEncoderShim::EncoderImpl::~EncoderImpl() {
  if (initialized_)
    vpx_codec_destroy(&encoder_);
}

void VideoEncoderShim::EncoderImpl::Initialize(const Config& config) {
  gfx::Size coded_size = media::VideoFrame::PlaneSize(
      config.input_format, 0, config.input_visible_size);

  // Only VP9 profile 0 is supported by PPAPI at the moment. VP9 profiles 1-3
  // are not supported due to backward compatibility.
  DCHECK_NE(config.output_profile, media::VP9PROFILE_PROFILE1);
  DCHECK_NE(config.output_profile, media::VP9PROFILE_PROFILE2);
  DCHECK_NE(config.output_profile, media::VP9PROFILE_PROFILE3);

  vpx_codec_iface_t* vpx_codec;
  int32_t min_quantizer, max_quantizer, cpu_used;
  GetVpxCodecParameters(config.output_profile, &vpx_codec, &min_quantizer,
                        &max_quantizer, &cpu_used);

  // Populate encoder configuration with default values.
  if (vpx_codec_enc_config_default(vpx_codec, &config_, 0) != VPX_CODEC_OK) {
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  config_.g_w = config.input_visible_size.width();
  config_.g_h = config.input_visible_size.height();

  framerate_ = config_.g_timebase.den;

  config_.g_lag_in_frames = 0;
  config_.g_timebase.num = 1;
  config_.g_timebase.den = base::Time::kMicrosecondsPerSecond;
  config_.rc_target_bitrate = config.initial_bitrate / 1000;
  config_.rc_min_quantizer = min_quantizer;
  config_.rc_max_quantizer = max_quantizer;
  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  config_.g_threads =
      std::min(kMaxNumThreads, (base::SysInfo::NumberOfProcessors() + 1) / 2);

  // Use Q/CQ mode if no target bitrate is given. Note that in the VP8/CQ case
  // the meaning of rc_target_bitrate changes to target maximum rate.
  if (config.initial_bitrate == 0) {
    if (config.output_profile == media::VP9PROFILE_PROFILE0) {
      config_.rc_end_usage = VPX_Q;
    } else if (config.output_profile == media::VP8PROFILE_ANY) {
      config_.rc_end_usage = VPX_CQ;
      config_.rc_target_bitrate = kVp8MaxCQBitrate;
    }
  }

  vpx_codec_flags_t flags = 0;
  if (vpx_codec_enc_init(&encoder_, vpx_codec, &config_, flags) !=
      VPX_CODEC_OK) {
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  initialized_ = true;

  if (vpx_codec_enc_config_set(&encoder_, &config_) != VPX_CODEC_OK) {
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  if (vpx_codec_control(&encoder_, VP8E_SET_CPUUSED, cpu_used) !=
      VPX_CODEC_OK) {
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  if (config.output_profile == media::VP9PROFILE_PROFILE0) {
    if (vpx_codec_control(&encoder_, VP9E_SET_AQ_MODE,
                          kVp9AqModeCyclicRefresh) != VPX_CODEC_OK) {
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }
  }

  renderer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoderShim::OnRequireBitstreamBuffers, shim_,
                     kInputFrameCount, coded_size, kBitstreamBufferSize));
}

void VideoEncoderShim::EncoderImpl::Encode(
    scoped_refptr<media::VideoFrame> frame,
    bool force_keyframe) {
  frames_.push_back(PendingEncode(std::move(frame), force_keyframe));
  DoEncode();
}

void VideoEncoderShim::EncoderImpl::UseOutputBitstreamBuffer(
    media::BitstreamBuffer buffer,
    uint8_t* mem) {
  buffers_.emplace_back(std::move(buffer), mem);
  DoEncode();
}

void VideoEncoderShim::EncoderImpl::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  framerate_ = framerate;

  uint32_t bitrate_kbit = bitrate / 1000;
  if (config_.rc_target_bitrate == bitrate_kbit)
    return;

  config_.rc_target_bitrate = bitrate_kbit;
  if (vpx_codec_enc_config_set(&encoder_, &config_) != VPX_CODEC_OK)
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
}

void VideoEncoderShim::EncoderImpl::Stop() {
  // Release frames on the renderer thread.
  while (!frames_.empty()) {
    PendingEncode frame = frames_.front();
    frames_.pop_front();

    renderer_task_runner_->ReleaseSoon(FROM_HERE, std::move(frame.frame));
  }
  buffers_.clear();
}

void VideoEncoderShim::EncoderImpl::DoEncode() {
  while (!frames_.empty() && !buffers_.empty()) {
    PendingEncode frame = frames_.front();
    frames_.pop_front();

    // Wrapper for vpx_codec_encode() to access the YUV data in the
    // |video_frame|. Only the VISIBLE rectangle within |video_frame|
    // is exposed to the codec.
    vpx_image_t vpx_image;
    vpx_image_t* const result = vpx_img_wrap(
        &vpx_image, VPX_IMG_FMT_I420, frame.frame->visible_rect().width(),
        frame.frame->visible_rect().height(), 1,
        frame.frame->data(media::VideoFrame::kYPlane));
    DCHECK_EQ(result, &vpx_image);
    vpx_image.planes[VPX_PLANE_Y] =
        frame.frame->visible_data(media::VideoFrame::kYPlane);
    vpx_image.planes[VPX_PLANE_U] =
        frame.frame->visible_data(media::VideoFrame::kUPlane);
    vpx_image.planes[VPX_PLANE_V] =
        frame.frame->visible_data(media::VideoFrame::kVPlane);
    vpx_image.stride[VPX_PLANE_Y] =
        frame.frame->stride(media::VideoFrame::kYPlane);
    vpx_image.stride[VPX_PLANE_U] =
        frame.frame->stride(media::VideoFrame::kUPlane);
    vpx_image.stride[VPX_PLANE_V] =
        frame.frame->stride(media::VideoFrame::kVPlane);

    vpx_codec_flags_t flags = 0;
    if (frame.force_keyframe)
      flags = VPX_EFLAG_FORCE_KF;

    const base::TimeDelta frame_duration =
        base::TimeDelta::FromSecondsD(1.0 / framerate_);
    if (vpx_codec_encode(&encoder_, &vpx_image, 0,
                         frame_duration.InMicroseconds(), flags,
                         VPX_DL_REALTIME) != VPX_CODEC_OK) {
      NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
      return;
    }

    const vpx_codec_cx_pkt_t* packet = nullptr;
    vpx_codec_iter_t iter = nullptr;
    while ((packet = vpx_codec_get_cx_data(&encoder_, &iter)) != nullptr) {
      if (packet->kind != VPX_CODEC_CX_FRAME_PKT)
        continue;

      BitstreamBuffer buffer = std::move(buffers_.front());
      buffers_.pop_front();

      CHECK(buffer.buffer.size() >= packet->data.frame.sz);
      memcpy(buffer.mem, packet->data.frame.buf, packet->data.frame.sz);

      // Pass the media::VideoFrame back to the renderer thread so it's
      // freed on the right thread.
      renderer_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&VideoEncoderShim::OnBitstreamBufferReady, shim_,
                         frame.frame, buffer.buffer.id(),
                         base::checked_cast<size_t>(packet->data.frame.sz),
                         (packet->data.frame.flags & VPX_FRAME_IS_KEY) != 0));
      break;  // Done, since all data is provided in one CX_FRAME_PKT packet.
    }
  }
}

void VideoEncoderShim::EncoderImpl::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  renderer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoderShim::OnNotifyError, shim_, error));
  Stop();
}

VideoEncoderShim::VideoEncoderShim(PepperVideoEncoderHost* host)
    : host_(host),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaThreadTaskRunner()) {
  encoder_impl_.reset(new EncoderImpl(weak_ptr_factory_.GetWeakPtr()));
}

VideoEncoderShim::~VideoEncoderShim() {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderShim::EncoderImpl::Stop,
                                base::Owned(encoder_impl_.release())));
}

media::VideoEncodeAccelerator::SupportedProfiles
VideoEncoderShim::GetSupportedProfiles() {
  media::VideoEncodeAccelerator::SupportedProfiles profiles;

  // Get the default VP8 config from Libvpx.
  vpx_codec_enc_cfg_t config;
  vpx_codec_err_t ret =
      vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &config, 0);
  if (ret == VPX_CODEC_OK) {
    media::VideoEncodeAccelerator::SupportedProfile profile;
    profile.profile = media::VP8PROFILE_ANY;
    profile.max_resolution = gfx::Size(kMaxWidth, kMaxHeight);
    // Libvpx and media::VideoEncodeAccelerator are using opposite
    // notions of denominator/numerator.
    profile.max_framerate_numerator = config.g_timebase.den;
    profile.max_framerate_denominator = config.g_timebase.num;
    profiles.push_back(profile);
  }

  ret = vpx_codec_enc_config_default(vpx_codec_vp9_cx(), &config, 0);
  if (ret == VPX_CODEC_OK) {
    media::VideoEncodeAccelerator::SupportedProfile profile;
    profile.max_resolution = gfx::Size(kMaxWidth, kMaxHeight);
    profile.max_framerate_numerator = config.g_timebase.den;
    profile.max_framerate_denominator = config.g_timebase.num;
    profile.profile = media::VP9PROFILE_PROFILE0;
    profiles.push_back(profile);
  }

  return profiles;
}

bool VideoEncoderShim::Initialize(
    const media::VideoEncodeAccelerator::Config& config,
    media::VideoEncodeAccelerator::Client* client) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(client, host_);

  if (config.input_format != media::PIXEL_FORMAT_I420)
    return false;

  if (config.output_profile != media::VP8PROFILE_ANY &&
      config.output_profile != media::VP9PROFILE_PROFILE0)
    return false;

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderShim::EncoderImpl::Initialize,
                                base::Unretained(encoder_impl_.get()), config));

  return true;
}

void VideoEncoderShim::Encode(scoped_refptr<media::VideoFrame> frame,
                              bool force_keyframe) {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderShim::EncoderImpl::Encode,
                                base::Unretained(encoder_impl_.get()),
                                std::move(frame), force_keyframe));
}

void VideoEncoderShim::UseOutputBitstreamBuffer(media::BitstreamBuffer buffer) {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoderShim::EncoderImpl::UseOutputBitstreamBuffer,
                     base::Unretained(encoder_impl_.get()), std::move(buffer),
                     host_->ShmHandleToAddress(buffer.id())));
}

void VideoEncoderShim::RequestEncodingParametersChange(uint32_t bitrate,
                                                       uint32_t framerate) {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoEncoderShim::EncoderImpl::RequestEncodingParametersChange,
          base::Unretained(encoder_impl_.get()), bitrate, framerate));
}

void VideoEncoderShim::Destroy() {
  DCHECK(RenderThreadImpl::current());

  delete this;
}

void VideoEncoderShim::OnRequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK(RenderThreadImpl::current());

  host_->RequireBitstreamBuffers(input_count, input_coded_size,
                                 output_buffer_size);
}

void VideoEncoderShim::OnBitstreamBufferReady(
    scoped_refptr<media::VideoFrame> frame,
    int32_t bitstream_buffer_id,
    size_t payload_size,
    bool key_frame) {
  DCHECK(RenderThreadImpl::current());

  host_->BitstreamBufferReady(bitstream_buffer_id,
                              media::BitstreamBufferMetadata(
                                  payload_size, key_frame, frame->timestamp()));
}

void VideoEncoderShim::OnNotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK(RenderThreadImpl::current());

  host_->NotifyError(error);
}

}  // namespace content
