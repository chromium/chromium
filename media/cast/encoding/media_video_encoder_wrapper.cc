// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/media_video_encoder_wrapper.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/types/cxx23_to_underlying.h"
#include "media/base/async_destroy_video_encoder.h"
#include "media/base/encoder_status.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/encoding/fake_software_video_encoder.h"
#include "media/cast/encoding/video_encoder.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "media/video/video_encoder_info.h"
#include "third_party/libaom/libaom_buildflags.h"

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/video/vpx_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif  // BUILDFLAG(ENABLE_LIBAOM)

namespace media::cast {
namespace {

// TODO(crbug.com/282984511): consider adding support for H264 here, using
// the media::OpenH264VideoEncoder.
std::unique_ptr<media::VideoEncoder> CreateSoftwareEncoder(VideoCodec codec) {
  switch (codec) {
#if BUILDFLAG(ENABLE_LIBVPX)
    case VideoCodec::kVP8:
    case VideoCodec::kVP9:
      return std::make_unique<media::VpxVideoEncoder>();
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
    case VideoCodec::kAV1:
      return std::make_unique<media::Av1VideoEncoder>();
#endif  // BUILDFLAG(ENABLE_LIBAOM)
    default:
      NOTREACHED() << "Unhandled codec. value=" << base::to_underlying(codec);
  }
}

VideoCodecProfile ToProfile(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return H264PROFILE_MAIN;
    case VideoCodec::kHEVC:
      return HEVCPROFILE_MAIN;
    case VideoCodec::kVP8:
      return VP8PROFILE_ANY;
    case VideoCodec::kVP9:
      // VP9 Profile 0 is 8 bit/sample at 4:2:0.
      return VP9PROFILE_PROFILE0;
    case VideoCodec::kAV1:
      return AV1PROFILE_PROFILE_MAIN;
    default:
      NOTREACHED() << "Unhandled codec. value=" << base::to_underlying(codec);
  }
}

void InitializeEncoderOnEncoderThread(
    const scoped_refptr<CastEnvironment>& environment,
    media::VideoEncoder& encoder,
    VideoCodecProfile profile,
    const media::VideoEncoder::Options& options,
    media::VideoEncoder::EncoderInfoCB info_cb,
    media::VideoEncoder::OutputCB output_cb,
    media::VideoEncoder::EncoderStatusCB done_cb) {
  CHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));

  encoder.Initialize(profile, options, std::move(info_cb), std::move(output_cb),
                     std::move(done_cb));

  // Our callbacks post to the correct thread, instead of needing the encoder to
  // manage posting back.
  encoder.DisablePostedCallbacks();
}

void EncodeVideoFrameOnEncoderThread(
    scoped_refptr<CastEnvironment> environment,
    media::VideoEncoder& encoder,
    scoped_refptr<media::VideoFrame> video_frame,
    const media::VideoEncoder::EncodeOptions& encode_options,
    media::VideoEncoder::EncoderStatusCB done_cb) {
  CHECK(environment->CurrentlyOn(CastEnvironment::VIDEO));
  encoder.Encode(std::move(video_frame), encode_options, std::move(done_cb));
}

// TODO(crbug.com/282984511): just use EncoderStatus directly once we remove
// media::cast::VideoEncoder.
OperationalStatus ToOperationalStatus(EncoderStatus status) {
  switch (status.code()) {
    case EncoderStatus::Codes::kOk:
      return STATUS_INITIALIZED;

    case EncoderStatus::Codes::kEncoderInitializeNeverCompleted:
    case EncoderStatus::Codes::kEncoderInitializeTwice:
    case EncoderStatus::Codes::kEncoderInitializationError:
      return STATUS_CODEC_INIT_FAILED;

    case EncoderStatus::Codes::kEncoderUnsupportedProfile:
    case EncoderStatus::Codes::kEncoderUnsupportedCodec:
      return STATUS_UNSUPPORTED_CODEC;

    case EncoderStatus::Codes::kEncoderUnsupportedConfig:
      return STATUS_INVALID_CONFIGURATION;

    // Most encoder statuses can just be considered runtime errors.
    default:
      return STATUS_CODEC_RUNTIME_ERROR;
  }
}

}  // namespace

MediaVideoEncoderWrapper::MediaVideoEncoderWrapper(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
    StatusChangeCallback status_change_cb,
    FrameEncodedCallback output_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb)
    : cast_environment_(std::move(cast_environment)),
      metrics_provider_(std::move(metrics_provider)),
      status_change_cb_(std::move(status_change_cb)),
      output_cb_(std::move(output_cb)),
      is_hardware_encoder_(video_config.use_hardware_encoder),
      codec_(video_config.video_codec()) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(cast_environment_->HasVideoThread());
  CHECK(status_change_cb_);
  CHECK(output_cb_);

  encode_options_.key_frame = true;
  options_.bitrate = Bitrate::ConstantBitrate(
      base::checked_cast<uint32_t>(video_config.start_bitrate));

  // NOTE: since we don't actually know the frame size until the first frame,
  // the encoder will not get created until the first call to
  // `EncodeVideoFrame`. However, it is ready to start receiving frames.
  status_change_cb_.Run(STATUS_INITIALIZED);
}

MediaVideoEncoderWrapper::~MediaVideoEncoderWrapper() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (encoder_) {
    cast_environment_->GetTaskRunner(CastEnvironment::VIDEO)
        ->DeleteSoon(FROM_HERE, encoder_.release());
  }
  weak_factory_.InvalidateWeakPtrs();
}

bool MediaVideoEncoderWrapper::EncodeVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(!video_frame->visible_rect().IsEmpty());

  // Construct and initialize the encoder on the first call to this method.
  const gfx::Size frame_size = video_frame->visible_rect().size();
  if (frame_size != options_.frame_size) {
    options_.frame_size = frame_size;
    ConstructEncoder();
    // TODO(crbug.com/282984511): add optimization for when we can reuse the
    // encoder at a different frame size. For example, software VP8, VP9, and
    // AV1 allow re-use if the new frame size is smaller.
  }

  recent_metadata_.emplace(CachedMetadata{
      .capture_begin_time = video_frame->metadata().capture_begin_time,
      .capture_end_time = video_frame->metadata().capture_end_time,
      .encode_start_time = base::TimeTicks::Now(),
      .rtp_timestamp =
          ToRtpTimeTicks(video_frame->timestamp(), kVideoFrequency),
      .reference_time = reference_time,
      .frame_duration = GetFrameDuration(*video_frame)});

  // Now that `GetFrameDuration` has been called, we can update the last frame
  // timestamp. It must be monotonically increasing.
  if (last_frame_timestamp_) {
    CHECK_GT(video_frame->timestamp(), last_frame_timestamp_.value());
  }
  last_frame_timestamp_ = video_frame->timestamp();

  cast_environment_->PostTask(
      CastEnvironment::VIDEO, FROM_HERE,
      base::BindOnce(&EncodeVideoFrameOnEncoderThread, cast_environment_,
                     std::ref(*encoder_), std::move(video_frame),
                     encode_options_, GetDoneCB()));
  encode_options_.key_frame = false;
  return true;
}

// Inform the encoder about the new target bit rate.
void MediaVideoEncoderWrapper::SetBitRate(int new_bit_rate) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  options_.bitrate =
      Bitrate::ConstantBitrate(base::checked_cast<uint32_t>(new_bit_rate));

  // If this method is called before the encoder_ is constructed, the bitrate
  // will be set as part of construction.
  if (encoder_) {
    UpdateEncoderOptions();
  }
}

// Inform the encoder to encode the next frame as a key frame.
void MediaVideoEncoderWrapper::GenerateKeyFrame() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  encode_options_.key_frame = true;
}

void MediaVideoEncoderWrapper::OnEncodedFrame(
    VideoEncoderOutput output,
    std::optional<media::VideoEncoder::CodecDescription> description) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  const CachedMetadata& metadata = recent_metadata_.front();
  auto encoded_frame = std::make_unique<SenderEncodedFrame>();
  encoded_frame->is_key_frame = output.key_frame;
  encoded_frame->frame_id = ++next_frame_id_;
  encoded_frame->referenced_frame_id = encoded_frame->is_key_frame
                                           ? encoded_frame->frame_id
                                           : encoded_frame->frame_id - 1;
  encoded_frame->rtp_timestamp = metadata.rtp_timestamp;
  encoded_frame->reference_time = metadata.reference_time;

  encoded_frame->encode_completion_time =
      cast_environment_->Clock()->NowTicks();

  // TODO(crbug.com/282984511): generalize logic for encoder related metrics.
  // This is based heavily on the logic in media/cast/encoding/vpx_encoder.cc.
  const base::TimeDelta processing_time =
      encoded_frame->encode_completion_time - metadata.encode_start_time;
  encoded_frame->encoder_utilization =
      processing_time / metadata.frame_duration;

  // TODO(crbug.com/282984511): determine if we need to adopt media::cast's
  // QuantizerEstimator in order to calculate lossiness. Currently, we just pass
  // a value of zero, which causes it to be ignored by the VideoSender's
  // feedback logic.
  encoded_frame->lossiness = 0.0f;

  encoded_frame->capture_begin_time = metadata.capture_begin_time;
  encoded_frame->capture_end_time = metadata.capture_end_time;
  encoded_frame->data = std::move(output.data);

  recent_metadata_.pop();
  metrics_provider_->IncrementEncodedFrameCount();
  output_cb_.Run(std::move(encoded_frame));
}

void MediaVideoEncoderWrapper::OnEncoderStatus(EncoderStatus error) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!last_recorded_status_ || error != last_recorded_status_.value()) {
    last_recorded_status_ = error;
    status_change_cb_.Run(ToOperationalStatus(error));

    if (!error.is_ok()) {
      metrics_provider_->SetError(std::move(error));
    }
  }
}

void MediaVideoEncoderWrapper::OnEncoderInfo(
    const VideoEncoderInfo& encoder_info) {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // TODO(crbug.com/282984511): support handling `supports_frame_size_change`
  // property.
}

void MediaVideoEncoderWrapper::ConstructEncoder() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // TODO(crbug.com/282984511): consider adding a fake software encoder for
  // testing.
  if (is_hardware_encoder_) {
    // TODO(crbug.com/282984511): write a GVAF impl for components/mirroring.
    NOTREACHED();
  } else {
    encoder_ = CreateSoftwareEncoder(codec_);
  }
  CHECK(encoder_);

  const VideoCodecProfile profile = ToProfile(codec_);
  metrics_provider_->Initialize(profile, options_.frame_size,
                                is_hardware_encoder_);
  cast_environment_->PostTask(
      CastEnvironment::VIDEO, FROM_HERE,
      base::BindOnce(&InitializeEncoderOnEncoderThread, cast_environment_,
                     std::ref(*encoder_), profile, options_, GetInfoCB(),
                     GetOutputCB(), GetDoneCB()));
}

base::TimeDelta MediaVideoEncoderWrapper::GetFrameDuration(
    const VideoFrame& frame) {
  // Frame has duration in metadata, use it.
  if (frame.metadata().frame_duration.has_value()) {
    return frame.metadata().frame_duration.value();
  }

  // No real way to figure out duration, use time passed since the last frame
  // as an educated guess, but clamp it within reasonable limits.
  constexpr auto min_duration = base::Seconds(1.0 / 60.0);
  constexpr auto max_duration = base::Seconds(1.0 / 24.0);
  const base::TimeDelta duration =
      frame.timestamp() - last_frame_timestamp_.value_or(base::TimeDelta{});
  return std::clamp(duration, min_duration, max_duration);
}

void MediaVideoEncoderWrapper::UpdateEncoderOptions() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->PostTask(
      CastEnvironment::VIDEO, FROM_HERE,
      base::BindOnce(
          [](media::VideoEncoder& encoder, media::VideoEncoder::Options options,
             media::VideoEncoder::OutputCB output_cb,
             media::VideoEncoder::EncoderStatusCB done_cb) {
            encoder.ChangeOptions(std::move(options), std::move(output_cb),
                                  std::move(done_cb));
          },
          // NOTE: Raw reference here is safe because the encoder is deleted in
          // a task posted to the video thread.
          std::ref(*encoder_), options_, GetOutputCB(), GetDoneCB()));
}

media::VideoEncoder::EncoderInfoCB MediaVideoEncoderWrapper::GetInfoCB() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return base::BindRepeating(
      [](base::WeakPtr<MediaVideoEncoderWrapper> weak_ptr,
         scoped_refptr<CastEnvironment> environment,
         const VideoEncoderInfo& encoder_info) {
        environment->PostTask(
            CastEnvironment::MAIN, FROM_HERE,
            base::BindOnce(&MediaVideoEncoderWrapper::OnEncoderInfo, weak_ptr,
                           encoder_info));
      },
      weak_factory_.GetWeakPtr(), cast_environment_);
}

media::VideoEncoder::OutputCB MediaVideoEncoderWrapper::GetOutputCB() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return base::BindRepeating(
      [](base::WeakPtr<MediaVideoEncoderWrapper> weak_ptr,
         scoped_refptr<CastEnvironment> environment, VideoEncoderOutput output,
         std::optional<media::VideoEncoder::CodecDescription> description) {
        environment->PostTask(
            CastEnvironment::MAIN, FROM_HERE,
            base::BindOnce(&MediaVideoEncoderWrapper::OnEncodedFrame, weak_ptr,
                           std::move(output), std::move(description)));
      },
      weak_factory_.GetWeakPtr(), cast_environment_);
}

media::VideoEncoder::EncoderStatusCB MediaVideoEncoderWrapper::GetDoneCB() {
  CHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return base::BindOnce(
      [](base::WeakPtr<MediaVideoEncoderWrapper> weak_ptr,
         scoped_refptr<CastEnvironment> environment, EncoderStatus status) {
        environment->PostTask(
            CastEnvironment::MAIN, FROM_HERE,
            base::BindOnce(&MediaVideoEncoderWrapper::OnEncoderStatus, weak_ptr,
                           status));
      },
      weak_factory_.GetWeakPtr(), cast_environment_);
}

}  //  namespace media::cast
