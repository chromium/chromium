// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/video_encoder_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "third_party/libaom/libaom_buildflags.h"
#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/cast/encoding/av1_encoder.h"
#endif
#include "media/base/video_encoder_metrics_provider.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/encoding/fake_software_video_encoder.h"
#include "media/cast/encoding/vpx_encoder.h"

namespace media {
namespace cast {

namespace {

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
    base::TimeTicks reference_time,
    const VideoEncoderImpl::CodecDynamicConfig& dynamic_config,
    VideoEncoderImpl::FrameEncodedCallback frame_encoded_callback) {
  DCHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));
  if (dynamic_config.key_frame_requested) {
    encoder->GenerateKeyFrame();
  }
  encoder->UpdateRates(dynamic_config.bit_rate);

  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->capture_begin_time =
      video_frame->metadata().capture_begin_time;
  encoded_frame->capture_end_time = video_frame->metadata().capture_end_time;
  encoder->Encode(std::move(video_frame), reference_time, encoded_frame.get());
  encoded_frame->encode_completion_time = environment->Clock()->NowTicks();
  environment->PostTask(CastEnvironment::MAIN, FROM_HERE,
                        base::BindOnce(std::move(frame_encoded_callback),
                                       std::move(encoded_frame)));
}
}  // namespace

VideoEncoderImpl::VideoEncoderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
    StatusChangeCallback status_change_cb)
    : cast_environment_(cast_environment) {
  CHECK(cast_environment_->HasVideoThread());
  DCHECK(status_change_cb);

  VideoCodec codec = video_config.video_codec();
  if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9) {
    encoder_ =
        std::make_unique<VpxEncoder>(video_config, std::move(metrics_provider));
    cast_environment_->PostTask(
        CastEnvironment::VIDEO, FROM_HERE,
        base::BindOnce(&InitializeEncoderOnEncoderThread, cast_environment,
                       encoder_.get()));
  } else if (codec == VideoCodec::kUnknown &&
             video_config.video_codec_params.value()
                 .enable_fake_codec_for_tests) {
    encoder_ = std::make_unique<FakeSoftwareVideoEncoder>(video_config);
#if BUILDFLAG(ENABLE_LIBAOM)
  } else if (codec == VideoCodec::kAV1) {
    encoder_ =
        std::make_unique<Av1Encoder>(video_config, std::move(metrics_provider));
    cast_environment_->PostTask(
        CastEnvironment::VIDEO, FROM_HERE,
        base::BindOnce(&InitializeEncoderOnEncoderThread, cast_environment,
                       encoder_.get()));
#endif
  } else {
    DCHECK(false) << "Invalid config";  // Codec not supported.
  }

  dynamic_config_.key_frame_requested = false;
  dynamic_config_.bit_rate = video_config.start_bitrate;

  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::BindOnce(
          std::move(status_change_cb),
          encoder_.get() ? STATUS_INITIALIZED : STATUS_UNSUPPORTED_CODEC));
}

VideoEncoderImpl::~VideoEncoderImpl() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (encoder_) {
    cast_environment_->PostTask(
        CastEnvironment::VIDEO, FROM_HERE,
        base::BindOnce(&base::DeletePointer<SoftwareVideoEncoder>,
                       encoder_.release()));
  }
}

bool VideoEncoderImpl::EncodeVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time,
    FrameEncodedCallback frame_encoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!video_frame->visible_rect().IsEmpty());
  DCHECK(!frame_encoded_callback.is_null());

  cast_environment_->PostTask(
      CastEnvironment::VIDEO, FROM_HERE,
      base::BindOnce(&EncodeVideoFrameOnEncoderThread, cast_environment_,
                     encoder_.get(), std::move(video_frame), reference_time,
                     dynamic_config_, std::move(frame_encoded_callback)));

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
