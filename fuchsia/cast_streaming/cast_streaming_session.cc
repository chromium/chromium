// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/public/cast_streaming_session.h"

#include <lib/zx/time.h>

#include "base/bind.h"
#include "base/notreached.h"
#include "base/timer/timer.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/network_util.h"
#include "components/openscreen_platform/task_runner.h"
#include "fuchsia/cast_streaming/cast_message_port_impl.h"
#include "fuchsia/cast_streaming/stream_consumer.h"
#include "media/base/media_util.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/openscreen/src/cast/streaming/receiver.h"
#include "third_party/openscreen/src/cast/streaming/receiver_session.h"

namespace {

// TODO(b/156117766): Remove these when Open Screen returns enum values rather
// than strings.
constexpr char kVideoCodecH264[] = "h264";
constexpr char kVideoCodecVp8[] = "vp8";

// Timeout to end the Session when no offer message is sent.
constexpr base::TimeDelta kInitTimeout = base::TimeDelta::FromSeconds(5);

}  // namespace

namespace cast_streaming {

// static
void CastStreamingSession::SetNetworkContextGetter(
    NetworkContextGetter getter) {
  openscreen_platform::SetNetworkContextGetter(std::move(getter));
}

// Owns the Open Screen ReceiverSession. The Cast Streaming Session is tied to
// the lifespan of this object.
class CastStreamingSession::Internal
    : public openscreen::cast::ReceiverSession::Client {
 public:
  Internal(
      CastStreamingSession::Client* client,
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner),
        environment_(&openscreen::Clock::now, &task_runner_),
        cast_message_port_impl_(std::move(message_port_request)),
        client_(client) {
    DCHECK(task_runner);
    DCHECK(client_);

    // TODO(crbug.com/1087520): Add streaming session Constraints and
    // DisplayDescription.
    receiver_session_ = std::make_unique<openscreen::cast::ReceiverSession>(
        this, &environment_, &cast_message_port_impl_,
        openscreen::cast::ReceiverSession::Preferences(
            {openscreen::cast::VideoCodec::kH264,
             openscreen::cast::VideoCodec::kVp8},
            {openscreen::cast::AudioCodec::kAac,
             openscreen::cast::AudioCodec::kOpus}));

    init_timeout_timer_.Start(
        FROM_HERE, kInitTimeout,
        base::BindOnce(&CastStreamingSession::Internal::OnInitializationTimeout,
                       base::Unretained(this)));
  }

  ~Internal() final = default;

  Internal(const Internal&) = delete;
  Internal& operator=(const Internal&) = delete;

 private:
  void OnInitializationTimeout() {
    DVLOG(1) << __func__;
    DCHECK(!is_initialized_);
    client_->OnInitializationFailure();
    is_initialized_ = true;
  }

  // openscreen::cast::ReceiverSession::Client implementation.
  void OnNegotiated(
      const openscreen::cast::ReceiverSession* session,
      openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) final {
    DVLOG(1) << __func__;
    DCHECK_EQ(session, receiver_session_.get());
    init_timeout_timer_.Stop();

    if (is_initialized_) {
      // TODO(crbug.com/1116185): Handle multiple offer messages properly.
      return;
    }

    base::Optional<AudioStreamInfo> audio_stream_info;
    if (receivers.audio) {
      // Creare the audio data pipe.
      const MojoCreateDataPipeOptions data_pipe_options{
          sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE,
          1u /* element_num_bytes */,
          media::GetDefaultDecoderBufferConverterCapacity(
              media::DemuxerStream::Type::AUDIO)};
      mojo::ScopedDataPipeProducerHandle data_pipe_producer;
      mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
      MojoResult result = mojo::CreateDataPipe(
          &data_pipe_options, &data_pipe_producer, &data_pipe_consumer);
      if (result != MOJO_RESULT_OK) {
        client_->OnInitializationFailure();
        return;
      }

      // Initialize the audio consumer.
      // We can use unretained pointers here because StreamConsumer is owned by
      // this object and |client_| is guaranteed to outlive this object.
      audio_consumer_ = std::make_unique<StreamConsumer>(
          receivers.audio->receiver, std::move(data_pipe_producer),
          base::BindRepeating(
              &CastStreamingSession::Client::OnAudioBufferReceived,
              base::Unretained(client_)),
          base::BindOnce(&CastStreamingSession::Internal::OnDataTimeout,
                         base::Unretained(this)));

      // Gather data for the audio decoder config.
      media::ChannelLayout channel_layout =
          media::GuessChannelLayout(receivers.audio->receiver_config.channels);
      const std::string& audio_codec =
          receivers.audio->selected_stream.stream.codec_name;
      media::AudioCodec media_audio_codec =
          media::StringToAudioCodec(audio_codec);
      int samples_per_second = receivers.audio->receiver_config.rtp_timebase;

      audio_stream_info.emplace(AudioStreamInfo{
          media::AudioDecoderConfig(
              media_audio_codec, media::SampleFormat::kSampleFormatF32,
              channel_layout, samples_per_second, media::EmptyExtraData(),
              media::EncryptionScheme::kUnencrypted),
          std::move(data_pipe_consumer)});

      DVLOG(1) << "Initialized audio stream. "
               << audio_stream_info->decoder_config.AsHumanReadableString();
    }

    base::Optional<VideoStreamInfo> video_stream_info;
    if (receivers.video) {
      // Creare the video data pipe.
      const MojoCreateDataPipeOptions data_pipe_options{
          sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE,
          1u /* element_num_bytes */,
          media::GetDefaultDecoderBufferConverterCapacity(
              media::DemuxerStream::Type::VIDEO)};
      mojo::ScopedDataPipeProducerHandle data_pipe_producer;
      mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
      MojoResult result = mojo::CreateDataPipe(
          &data_pipe_options, &data_pipe_producer, &data_pipe_consumer);
      if (result != MOJO_RESULT_OK) {
        client_->OnInitializationFailure();
        return;
      }

      // Initialize the video consumer.
      // We can use unretained pointers here because StreamConsumer is owned by
      // this object and |client_| is guaranteed to outlive this object.
      video_consumer_ = std::make_unique<StreamConsumer>(
          receivers.video->receiver, std::move(data_pipe_producer),
          base::BindRepeating(
              &CastStreamingSession::Client::OnVideoBufferReceived,
              base::Unretained(client_)),
          base::BindOnce(&CastStreamingSession::Internal::OnDataTimeout,
                         base::Unretained(this)));

      // Gather data for the video decoder config.
      const std::string& video_codec =
          receivers.video->selected_stream.stream.codec_name;
      uint32_t video_width =
          receivers.video->selected_stream.resolutions[0].width;
      uint32_t video_height =
          receivers.video->selected_stream.resolutions[0].height;
      gfx::Size video_size(video_width, video_height);
      gfx::Rect video_rect(video_width, video_height);

      media::VideoCodec media_video_codec =
          media::VideoCodec::kUnknownVideoCodec;
      media::VideoCodecProfile video_codec_profile =
          media::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;

      if (video_codec == kVideoCodecH264) {
        media_video_codec = media::VideoCodec::kCodecH264;
        video_codec_profile = media::VideoCodecProfile::H264PROFILE_BASELINE;
      } else if (video_codec == kVideoCodecVp8) {
        media_video_codec = media::VideoCodec::kCodecVP8;
        video_codec_profile = media::VideoCodecProfile::VP8PROFILE_MIN;
      } else {
        NOTREACHED();
      }

      video_stream_info.emplace(VideoStreamInfo{
          media::VideoDecoderConfig(
              media_video_codec, video_codec_profile,
              media::VideoDecoderConfig::AlphaMode::kIsOpaque,
              media::VideoColorSpace(), media::VideoTransformation(),
              video_size, video_rect, video_size, media::EmptyExtraData(),
              media::EncryptionScheme::kUnencrypted),
          std::move(data_pipe_consumer)});

      DVLOG(1) << "Initialized video stream. "
               << video_stream_info->decoder_config.AsHumanReadableString();
    }

    if (!audio_stream_info && !video_stream_info) {
      client_->OnInitializationFailure();
    } else {
      client_->OnInitializationSuccess(std::move(audio_stream_info),
                                       std::move(video_stream_info));
    }
    is_initialized_ = true;
  }

  // TODO(https://crbug.com/1116185): Handle |reason| and reset streams on a
  // new offer message.
  void OnReceiversDestroying(const openscreen::cast::ReceiverSession* session,
                             ReceiversDestroyingReason reason) final {
    // This can be called when |receiver_session_| is being destroyed, so we
    // do not sanity-check |session| here.
    DVLOG(1) << __func__;
    audio_consumer_.reset();
    video_consumer_.reset();
    client_->OnReceiverSessionEnded();
  }

  void OnError(const openscreen::cast::ReceiverSession* session,
               openscreen::Error error) final {
    DCHECK_EQ(session, receiver_session_.get());
    LOG(ERROR) << error;
    if (!is_initialized_) {
      client_->OnInitializationFailure();
      is_initialized_ = true;
    }
  }

  void OnDataTimeout() {
    DVLOG(1) << __func__;
    receiver_session_.reset();
  }

  openscreen_platform::TaskRunner task_runner_;
  openscreen::cast::Environment environment_;
  CastMessagePortImpl cast_message_port_impl_;
  std::unique_ptr<openscreen::cast::ReceiverSession> receiver_session_;
  base::OneShotTimer init_timeout_timer_;

  bool is_initialized_ = false;
  CastStreamingSession::Client* const client_;
  std::unique_ptr<openscreen::cast::Receiver::Consumer> audio_consumer_;
  std::unique_ptr<openscreen::cast::Receiver::Consumer> video_consumer_;
};

CastStreamingSession::Client::~Client() = default;
CastStreamingSession::CastStreamingSession() = default;
CastStreamingSession::~CastStreamingSession() = default;

void CastStreamingSession::Start(
    Client* client,
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(client);
  DCHECK(!internal_);
  internal_ = std::make_unique<Internal>(
      client, std::move(message_port_request), task_runner);
}

void CastStreamingSession::Stop() {
  DCHECK(internal_);
  internal_.reset();
}

}  // namespace cast_streaming
