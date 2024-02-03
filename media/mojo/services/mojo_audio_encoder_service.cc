// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_encoder_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

MojoAudioEncoderService::MojoAudioEncoderService(
    std::unique_ptr<media::AudioEncoder> encoder)
    : encoder_(std::move(encoder)) {
  DCHECK(encoder_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioEncoderService::~MojoAudioEncoderService() = default;

void MojoAudioEncoderService::Initialize(
    mojo::PendingAssociatedRemote<mojom::AudioEncoderClient> client,
    const AudioEncoderConfig& config,
    InitializeCallback callback) {
  if (client_.is_bound()) {
    std::move(callback).Run(EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  client_.Bind(std::move(client));
  encoder_->Initialize(
      config,
      base::BindRepeating(&MojoAudioEncoderService::OnOutput, weak_this_),
      base::BindOnce(&MojoAudioEncoderService::OnDone, weak_this_,
                     std::move(callback)));
}

void MojoAudioEncoderService::Encode(mojom::AudioBufferPtr buffer,
                                     EncodeCallback callback) {
  if (!client_.is_bound() || !client_.is_connected()) {
    std::move(callback).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  auto audio_buffer = buffer.To<scoped_refptr<AudioBuffer>>();
  auto audio_bus = AudioBuffer::WrapOrCopyToAudioBus(audio_buffer);
  encoder_->Encode(std::move(audio_bus),
                   base::TimeTicks() + audio_buffer->timestamp(),
                   base::BindOnce(&MojoAudioEncoderService::OnDone, weak_this_,
                                  std::move(callback)));
}

void MojoAudioEncoderService::Flush(FlushCallback callback) {
  if (!client_.is_bound() || !client_.is_connected()) {
    std::move(callback).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  encoder_->Flush(base::BindOnce(&MojoAudioEncoderService::OnDone, weak_this_,
                                 std::move(callback)));
}

void MojoAudioEncoderService::OnDone(MojoDoneCallback callback,
                                     EncoderStatus error) {
  std::move(callback).Run(error);
}

void MojoAudioEncoderService::OnOutput(
    EncodedAudioBuffer output,
    std::optional<media::AudioEncoder::CodecDescription> desc) {
  client_->OnEncodedBufferReady(
      std::move(output),
      desc.value_or(media::AudioEncoder::CodecDescription()));
}

}  // namespace media
