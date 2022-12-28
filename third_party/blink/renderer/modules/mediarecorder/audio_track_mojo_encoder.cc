// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_mojo_encoder.h"

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/encoder_status.h"
#include "media/mojo/clients/mojo_audio_encoder.h"
#include "media/mojo/mojom/audio_encoder.mojom-blink.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {

void LogError(const std::string& message, media::EncoderStatus error) {
  DVLOG(1) << message << static_cast<int>(error.code()) << " "
           << error.message();
}

}  // namespace

namespace blink {

AudioTrackMojoEncoder::AudioTrackMojoEncoder(
    AudioTrackRecorder::CodecId codec,
    OnEncodedAudioCB on_encoded_audio_cb,
    uint32_t bits_per_second)
    : AudioTrackEncoder(std::move(on_encoded_audio_cb)),
      bits_per_second_(bits_per_second),
      current_status_(
          media::EncoderStatus::Codes::kEncoderInitializeNeverCompleted) {
  DCHECK_EQ(codec, AudioTrackRecorder::CodecId::kAac);
  codec_ = codec;
}

void AudioTrackMojoEncoder::OnSetFormat(
    const media::AudioParameters& input_params) {
  DVLOG(1) << __func__;
  if (input_params_.Equals(input_params) && current_status_.is_ok())
    return;

  current_status_ =
      media::EncoderStatus::Codes::kEncoderInitializeNeverCompleted;
  input_queue_ = base::queue<PendingData>();

  if (!input_params.IsValid()) {
    DVLOG(1) << "Invalid params: " << input_params.AsHumanReadableString();
    return;
  }
  input_params_ = input_params;

  // Encoding is done by the platform and runs in the GPU process. So we
  // must create a `MojoAudioEncoder` to communicate with the actual encoder.
  mojo::PendingRemote<media::mojom::InterfaceFactory> pending_interface_factory;
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory;
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      pending_interface_factory.InitWithNewPipeAndPassReceiver());
  interface_factory.Bind(std::move(pending_interface_factory));
  mojo::PendingRemote<media::mojom::AudioEncoder> encoder_remote;
  interface_factory->CreateAudioEncoder(
      encoder_remote.InitWithNewPipeAndPassReceiver());
  mojo_encoder_ =
      std::make_unique<media::MojoAudioEncoder>(std::move(encoder_remote));
  if (!mojo_encoder_) {
    DVLOG(1) << "Couldn't create Mojo encoder.";
    return;
  }

  media::AudioEncoder::Options options = {};
  if (codec_ == AudioTrackRecorder::CodecId::kAac) {
    options.codec = media::AudioCodec::kAAC;
  } else {
    DVLOG(1) << "Unsupported codec: " << static_cast<int>(codec_);
    return;
  }

  options.channels = input_params_.channels();
  options.sample_rate = input_params_.sample_rate();
  if (bits_per_second_ > 0)
    options.bitrate = bits_per_second_;

  auto output_cb = media::BindToCurrentLoop(WTF::BindRepeating(
      &AudioTrackMojoEncoder::OnEncodeOutput, weak_factory_.GetWeakPtr()));
  auto done_cb = media::BindToCurrentLoop(WTF::BindOnce(
      &AudioTrackMojoEncoder::OnInitializeDone, weak_factory_.GetWeakPtr()));
  mojo_encoder_->Initialize(options, std::move(output_cb), std::move(done_cb));
}

void AudioTrackMojoEncoder::EncodeAudio(
    std::unique_ptr<media::AudioBus> input_bus,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << ", #frames " << input_bus->frames();
  DCHECK_EQ(input_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());

  if (paused_)
    return;

  if (current_status_ ==
      media::EncoderStatus::Codes::kEncoderInitializeNeverCompleted) {
    input_queue_.push({std::move(input_bus), capture_time});
    return;
  }

  if (!current_status_.is_ok()) {
    LogError("EncodeAudio refused: ", current_status_);
    return;
  }

  auto done_cb = media::BindToCurrentLoop(WTF::BindOnce(
      &AudioTrackMojoEncoder::OnEncodeDone, weak_factory_.GetWeakPtr()));
  mojo_encoder_->Encode(std::move(input_bus), capture_time, std::move(done_cb));
}

void AudioTrackMojoEncoder::OnInitializeDone(media::EncoderStatus status) {
  // Don't override `current_status_` with `kOk` if we hit an error previously.
  if (status.is_ok() && !current_status_.is_ok() &&
      current_status_ !=
          media::EncoderStatus::Codes::kEncoderInitializeNeverCompleted) {
    return;
  }

  current_status_ = status;
  if (!current_status_.is_ok()) {
    LogError("Audio encoder initialization failed: ", current_status_);
    return;
  }

  while (!input_queue_.empty()) {
    EncodeAudio(std::move(input_queue_.front().audio_bus),
                input_queue_.front().capture_time);
    input_queue_.pop();
  }
}

void AudioTrackMojoEncoder::OnEncodeDone(media::EncoderStatus status) {
  // Don't override `current_status_` with `kOk` if we hit an error previously.
  if (status.is_ok() && !current_status_.is_ok())
    return;

  current_status_ = status;
  if (!current_status_.is_ok())
    LogError("Audio encode failed: ", current_status_);
}

void AudioTrackMojoEncoder::OnEncodeOutput(
    media::EncodedAudioBuffer encoded_buffer,
    absl::optional<media::AudioEncoder::CodecDescription> codec_desc) {
  if (!current_status_.is_ok()) {
    LogError("Refusing to output when in error state: ", current_status_);
    return;
  }

  std::string encoded_data(
      reinterpret_cast<char*>(encoded_buffer.encoded_data.get()),
      encoded_buffer.encoded_data_size);
  on_encoded_audio_cb_.Run(encoded_buffer.params, encoded_data,
                           encoded_buffer.timestamp);
}

}  // namespace blink
