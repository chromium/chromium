// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_encoder_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/cast/sender/fake_software_video_encoder.h"
#include "media/cast/sender/vp8_encoder.h"

namespace media {
namespace cast {

namespace {

typedef base::Callback<void(Vp8Encoder*)> PassEncoderCallback;

void InitializeEncoderOnEncoderThread(
    const scoped_refptr<CastEnvironment>& environment,
    SoftwareVideoEncoder* encoder) {
  DCHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));
  encoder->Initialize();
}

void EncodeVideoFrameOnEncoderThread(
    scoped_refptr<CastEnvironment> environment,
    SoftwareVideoEncoder* encoder,
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks& reference_time,
    const VideoEncoderImpl::CodecDynamicConfig& dynamic_config,
    const VideoEncoderImpl::FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));
  if (dynamic_config.key_frame_requested) {
    encoder->GenerateKeyFrame();
  }
  encoder->UpdateRates(dynamic_config.bit_rate);

  std::unique_ptr<SenderEncodedFrame> encoded_frame(new SenderEncodedFrame());
  encoder->Encode(std::move(video_frame), reference_time, encoded_frame.get());
  encoded_frame->encode_completion_time = environment->Clock()->NowTicks();
  environment->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(frame_encoded_callback, base::Passed(&encoded_frame)));
}
}  // namespace

// static
bool VideoEncoderImpl::IsSupported(const FrameSenderConfig& video_config) {
#ifndef OFFICIAL_BUILD
  if (video_config.codec == CODEC_VIDEO_FAKE)
    return true;
#endif
  return video_config.codec == CODEC_VIDEO_VP8;
}

VideoEncoderImpl::VideoEncoderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& video_config,
    const StatusChangeCallback& status_change_cb)
    : cast_environment_(cast_environment) {
  CHECK(cast_environment_->HasVideoThread());
  DCHECK(status_change_cb);

  if (video_config.codec == CODEC_VIDEO_VP8) {
    encoder_.reset(new Vp8Encoder(video_config));
    cast_environment_->PostTask(CastEnvironment::VIDEO,
                                FROM_HERE,
                                base::Bind(&InitializeEncoderOnEncoderThread,
                                           cast_environment,
                                           encoder_.get()));
#ifndef OFFICIAL_BUILD
  } else if (video_config.codec == CODEC_VIDEO_FAKE) {
    encoder_.reset(new FakeSoftwareVideoEncoder(video_config));
#endif
  } else {
    DCHECK(false) << "Invalid config";  // Codec not supported.
  }

  dynamic_config_.key_frame_requested = false;
  dynamic_config_.bit_rate = video_config.start_bitrate;

  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(status_change_cb,
                 encoder_.get() ? STATUS_INITIALIZED :
                                  STATUS_UNSUPPORTED_CODEC));
}

VideoEncoderImpl::~VideoEncoderImpl() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (encoder_) {
    cast_environment_->PostTask(
        CastEnvironment::VIDEO,
        FROM_HERE,
        base::Bind(&base::DeletePointer<SoftwareVideoEncoder>,
                   encoder_.release()));
  }
}

bool VideoEncoderImpl::EncodeVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks& reference_time,
    const FrameEncodedCallback& frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!video_frame->visible_rect().IsEmpty());
  DCHECK(!frame_encoded_callback.is_null());

  cast_environment_->PostTask(
      CastEnvironment::VIDEO, FROM_HERE,
      base::BindRepeating(&EncodeVideoFrameOnEncoderThread, cast_environment_,
                          encoder_.get(), std::move(video_frame),
                          reference_time, dynamic_config_,
                          frame_encoded_callback));

  dynamic_config_.key_frame_requested = false;
  return true;
}

// Inform the encoder about the new target bit rate.
void VideoEncoderImpl::SetBitRate(int new_bit_rate) {
  dynamic_config_.bit_rate = new_bit_rate;
}

// Inform the encoder to encode the next frame as a key frame.
void VideoEncoderImpl::GenerateKeyFrame() {
  dynamic_config_.key_frame_requested = true;
}

}  //  namespace cast
}  //  namespace media
