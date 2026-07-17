// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/devtools_media_encoding_service/devtools_media_encoding_service_impl.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/decoder_buffer.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/muxers/mp4_muxer.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/muxer_timestamp_adapter.h"
#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#include "media/video/offloading_video_encoder.h"
#endif

namespace content {

DevToolsMediaEncodingServiceImpl::DevToolsMediaEncodingServiceImpl(
    mojo::PendingReceiver<
        devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
        receiver)
    : receiver_(this, std::move(receiver)) {}

DevToolsMediaEncodingServiceImpl::~DevToolsMediaEncodingServiceImpl() {
  StopRecording();
  if (screencast_mp4_muxer_) {
    screencast_mp4_muxer_->Flush();
  }
}

void DevToolsMediaEncodingServiceImpl::StartRecording(
    mojo::PendingRemote<devtools_media_encoding_service::mojom::
                            DevToolsMediaEncodingServiceClient> client,
    uint32_t max_width,
    uint32_t max_height,
    uint32_t frame_rate,
    bool has_audio) {
#if !BUILDFLAG(ENABLE_LIBAOM)
  receiver_.ReportBadMessage("Video encoding is not supported");
#else
  if (client_.is_bound()) {
    receiver_.ReportBadMessage("Recording is already active");
    return;
  }
  if (frame_rate == 0) {
    frame_rate = 30;
  }
  client_.Bind(std::move(client));
  screencast_frame_rate_ = frame_rate;
  has_audio_ = has_audio;
  stopping_ = false;
  wait_for_queues_to_finish_ = false;
  last_video_frame_ = nullptr;
  video_encoder_flushing_ = false;

  auto delegate = std::make_unique<media::Mp4MuxerDelegate>(
      media::AudioCodec::kOpus, media::VideoCodec::kAV1,
      media::AV1PROFILE_PROFILE_MAIN, media::kNoVideoCodecLevel, false,
      base::BindRepeating(
          [](base::WeakPtr<DevToolsMediaEncodingServiceImpl> self,
             base::span<const uint8_t> data) {
            if (self && self->client_.is_bound()) {
              self->client_->OnData(
                  std::vector<uint8_t>(data.begin(), data.end()));
            }
          },
          weak_factory_.GetWeakPtr()));

  screencast_mp4_muxer_ = std::make_unique<media::MuxerTimestampAdapter>(
      std::make_unique<media::Mp4Muxer>(media::AudioCodec::kOpus, true,
                                        has_audio, std::move(delegate),
                                        std::nullopt),
      true, has_audio);

  last_surface_size_ = gfx::Size(max_width, max_height);
  screencast_video_encoder_ = std::make_unique<media::OffloadingVideoEncoder>(
      std::make_unique<media::Av1VideoEncoder>());
  media::VideoEncoder::Options options;
  options.frame_size = last_surface_size_;
  options.framerate = frame_rate;
  options.keyframe_interval = frame_rate * 2;

  screencast_video_encoder_->Initialize(
      media::AV1PROFILE_PROFILE_MAIN, options, base::DoNothing(),
      base::BindRepeating(
          &DevToolsMediaEncodingServiceImpl::OnScreencastEncodedFrame,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(
          [](base::WeakPtr<DevToolsMediaEncodingServiceImpl> self,
             media::EncoderStatus status) {
            if (!self || !self->screencast_mp4_muxer_ || self->stopping_) {
              return;
            }
            if (!status.is_ok()) {
              self->wait_for_queues_to_finish_ = false;
              self->last_video_frame_ = nullptr;
              self->video_frame_queue_.clear();
              self->StopRecording();
              return;
            }
            self->ProcessVideoFrameQueue();
          },
          weak_factory_.GetWeakPtr()));

#endif
}

void DevToolsMediaEncodingServiceImpl::RecordVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (!screencast_mp4_muxer_ || stopping_ || wait_for_queues_to_finish_ ||
      !screencast_video_encoder_ || !frame) {
    return;
  }
  last_video_frame_ = frame;
  last_video_frame_receive_time_ = base::TimeTicks::Now();
  video_frame_queue_.push_back(frame);
  ProcessVideoFrameQueue();
}

void DevToolsMediaEncodingServiceImpl::ProcessVideoFrameQueue() {
  if (!screencast_mp4_muxer_ || stopping_) {
    return;
  }

  while (!video_frame_queue_.empty()) {
    if (stopping_ || !screencast_video_encoder_ || video_encoder_flushing_) {
      break;
    }
    auto frame = video_frame_queue_.front();
    const gfx::Size surface_size = frame->visible_rect().size();
    if (surface_size != last_surface_size_) {
      last_surface_size_ = surface_size;
      video_encoder_flushing_ = true;

      screencast_video_encoder_->Flush(base::BindOnce(
          &DevToolsMediaEncodingServiceImpl::OnVideoEncoderFlushedAndRecreate,
          weak_factory_.GetWeakPtr()));
      return;
    }

    video_frame_queue_.pop_front();
    screencast_video_encoder_->Encode(
        frame, media::VideoEncoder::EncodeOptions(false),
        base::BindOnce(
            [](base::WeakPtr<DevToolsMediaEncodingServiceImpl> self,
               media::EncoderStatus status) {
              if (self) {
                if (!status.is_ok()) {
                  self->wait_for_queues_to_finish_ = false;
                  self->last_video_frame_ = nullptr;
                  self->video_frame_queue_.clear();
                  self->StopRecording();
                } else {
                  self->TryFlushEncoders();
                }
              }
            },
            weak_factory_.GetWeakPtr()));
  }

  TryFlushEncoders();
}

void DevToolsMediaEncodingServiceImpl::OnVideoEncoderFlushedAndRecreate(
    media::EncoderStatus status) {
  video_encoder_flushing_ = false;
  if (stopping_) {
    OnEncoderFlushed(status);
    return;
  }

  if (!status.is_ok()) {
    StopRecording();
    return;
  }

#if BUILDFLAG(ENABLE_LIBAOM)
  screencast_video_encoder_ = std::make_unique<media::OffloadingVideoEncoder>(
      std::make_unique<media::Av1VideoEncoder>());

  media::VideoEncoder::Options options;
  options.frame_size = last_surface_size_;
  options.framerate = screencast_frame_rate_;
  options.keyframe_interval = screencast_frame_rate_ * 2;

  screencast_video_encoder_->Initialize(
      media::AV1PROFILE_PROFILE_MAIN, options, base::DoNothing(),
      base::BindRepeating(
          &DevToolsMediaEncodingServiceImpl::OnScreencastEncodedFrame,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(
          [](base::WeakPtr<DevToolsMediaEncodingServiceImpl> self,
             media::EncoderStatus status) {
            if (!self || !self->screencast_mp4_muxer_ || self->stopping_) {
              return;
            }
            if (!status.is_ok()) {
              self->wait_for_queues_to_finish_ = false;
              self->last_video_frame_ = nullptr;
              self->video_frame_queue_.clear();
              self->StopRecording();
              return;
            }
            self->ProcessVideoFrameQueue();
          },
          weak_factory_.GetWeakPtr()));
#endif
}

void DevToolsMediaEncodingServiceImpl::RecordAudioBuffer(
    media::mojom::AudioBufferPtr buffer) {
  if (!screencast_mp4_muxer_ || stopping_ || wait_for_queues_to_finish_ ||
      !buffer || !has_audio_) {
    return;
  }

  auto audio_buffer =
      mojo::ConvertTo<scoped_refptr<media::AudioBuffer>>(std::move(buffer));
  if (!audio_buffer) {
    return;
  }

  if (!screencast_audio_encoder_) {
    screencast_audio_encoder_ = std::make_unique<media::OffloadingAudioEncoder>(
        std::make_unique<media::AudioOpusEncoder>());
    media::AudioEncoder::Options audio_options;
    audio_options.codec = media::AudioCodec::kOpus;
    audio_options.sample_rate = audio_buffer->sample_rate();
    audio_options.channels = audio_buffer->channel_count();
    audio_options.bitrate = 128000;

    screencast_audio_encoder_->Initialize(
        audio_options,
        base::BindRepeating(
            &DevToolsMediaEncodingServiceImpl::OnScreencastEncodedAudio,
            weak_factory_.GetWeakPtr()),
        base::DoNothing());
  }

  base::TimeTicks capture_time = base::TimeTicks() + audio_buffer->timestamp();
  auto audio_bus =
      media::AudioBuffer::WrapOrCopyToAudioBus(std::move(audio_buffer));
  if (!audio_bus) {
    return;
  }

  screencast_audio_encoder_->Encode(
      std::move(audio_bus), capture_time,
      base::BindOnce(
          [](base::WeakPtr<DevToolsMediaEncodingServiceImpl> self,
             media::EncoderStatus status) {
            if (self) {
              if (!status.is_ok()) {
                self->wait_for_queues_to_finish_ = false;
                self->last_video_frame_ = nullptr;
                self->video_frame_queue_.clear();
                self->StopRecording();
              } else {
                self->TryFlushEncoders();
              }
            }
          },
          weak_factory_.GetWeakPtr()));
  TryFlushEncoders();
}

void DevToolsMediaEncodingServiceImpl::TryFlushEncoders() {
  if (!wait_for_queues_to_finish_) {
    return;
  }
  if (!video_frame_queue_.empty() || video_encoder_flushing_) {
    return;
  }

  wait_for_queues_to_finish_ = false;
  stopping_ = true;

  auto* video_encoder = screencast_video_encoder_.get();
  auto* audio_encoder = screencast_audio_encoder_.get();

  int expected_flushes = 0;
  if (video_encoder) {
    expected_flushes++;
  }
  if (audio_encoder) {
    expected_flushes++;
  }

  encoders_flushing_ = expected_flushes;

  if (video_encoder && !video_encoder_flushing_) {
    video_encoder->Flush(
        base::BindOnce(&DevToolsMediaEncodingServiceImpl::OnEncoderFlushed,
                       weak_factory_.GetWeakPtr()));
  }
  if (audio_encoder) {
    audio_encoder->Flush(
        base::BindOnce(&DevToolsMediaEncodingServiceImpl::OnEncoderFlushed,
                       weak_factory_.GetWeakPtr()));
  }

  if (expected_flushes == 0) {
    if (screencast_mp4_muxer_) {
      screencast_mp4_muxer_->Flush();
    }
    screencast_mp4_muxer_.reset();
    screencast_video_encoder_.reset();
    screencast_audio_encoder_.reset();
    if (client_.is_bound()) {
      client_->OnClosed();
      client_.reset();
    }
    stopping_ = false;
  }
}

void DevToolsMediaEncodingServiceImpl::StopRecording() {
  if (!screencast_mp4_muxer_ || stopping_ || wait_for_queues_to_finish_) {
    return;
  }

  if (last_video_frame_) {
    base::TimeDelta delay =
        base::TimeTicks::Now() - last_video_frame_receive_time_;
    if (delay.is_positive()) {
      auto tail_frame = media::VideoFrame::WrapVideoFrame(
          last_video_frame_, last_video_frame_->format(),
          last_video_frame_->visible_rect(), last_video_frame_->natural_size());
      if (tail_frame) {
        tail_frame->set_timestamp(last_video_frame_->timestamp() + delay);
        video_frame_queue_.push_back(std::move(tail_frame));
      }
    }
  }

  wait_for_queues_to_finish_ = true;
  ProcessVideoFrameQueue();
  TryFlushEncoders();
}

void DevToolsMediaEncodingServiceImpl::OnEncoderFlushed(
    media::EncoderStatus status) {
  encoders_flushing_--;
  if (encoders_flushing_ <= 0) {
    encoders_flushing_ = 0;
    if (screencast_mp4_muxer_) {
      screencast_mp4_muxer_->Flush();
    }
    screencast_mp4_muxer_.reset();
    screencast_video_encoder_.reset();
    screencast_audio_encoder_.reset();
    if (client_.is_bound()) {
      client_->OnClosed();
      client_.reset();
    }
    stopping_ = false;
  }
}

void DevToolsMediaEncodingServiceImpl::OnScreencastEncodedAudio(
    media::EncodedAudioBuffer encoded_audio,
    std::optional<media::AudioEncoder::CodecDescription> description) {
  if (!screencast_mp4_muxer_ || encoded_audio.encoded_data.empty()) {
    return;
  }

  bool is_key_frame = true;
  base::TimeDelta timestamp = encoded_audio.timestamp - base::TimeTicks();

  auto decoder_buffer =
      media::DecoderBuffer::FromArray(std::move(encoded_audio.encoded_data));
  decoder_buffer->set_timestamp(timestamp);
  decoder_buffer->set_duration(encoded_audio.duration);
  decoder_buffer->set_is_key_frame(is_key_frame);

  screencast_mp4_muxer_->OnEncodedAudio(
      encoded_audio.params, std::move(decoder_buffer), std::move(description),
      encoded_audio.timestamp);
}

void DevToolsMediaEncodingServiceImpl::OnScreencastEncodedFrame(
    media::VideoEncoderOutput output,
    std::optional<media::VideoEncoder::CodecDescription> description) {
  if (!screencast_mp4_muxer_ || output.data.empty()) {
    return;
  }

  bool is_key_frame = output.key_frame;
  base::TimeDelta timestamp = output.timestamp;

  auto decoder_buffer = media::DecoderBuffer::FromArray(std::move(output.data));
  decoder_buffer->set_timestamp(timestamp);
  decoder_buffer->set_is_key_frame(is_key_frame);

  media::Muxer::VideoParameters params(
      output.encoded_size.value_or(last_surface_size_), screencast_frame_rate_,
      media::VideoCodec::kAV1, output.color_space, std::nullopt);

  screencast_mp4_muxer_->OnEncodedVideo(params, std::move(decoder_buffer),
                                        std::move(description),
                                        base::TimeTicks() + timestamp);
}

}  // namespace content
