// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/output_device.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/threading/thread_restrictions.h"
#include "media/audio/audio_output_device_thread_callback.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace audio {

OutputDevice::OutputDevice(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const media::AudioParameters& params,
    media::AudioRendererSink::RenderCallback* render_callback,
    const std::string& device_id)
    : audio_parameters_(params),
      render_callback_(render_callback),
      stream_factory_(std::move(stream_factory)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());

  stream_factory_->CreateOutputStream(
      stream_.BindNewPipeAndPassReceiver(), mojo::NullAssociatedRemote(),
      mojo::NullRemote(), device_id, params, base::UnguessableToken::Create(),
      base::BindOnce(&OutputDevice::StreamCreated, weak_factory_.GetWeakPtr()));
  stream_.set_disconnect_handler(base::BindOnce(
      &OutputDevice::OnConnectionError, weak_factory_.GetWeakPtr()));
}

OutputDevice::~OutputDevice() {
  CleanUp();
}

void OutputDevice::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_.is_bound()) {
    stream_->Play();
  }
}

void OutputDevice::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_.is_bound()) {
    stream_->Pause();
  }
}

void OutputDevice::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_.is_bound()) {
    stream_->SetVolume(volume);
  }
}

void OutputDevice::StreamCreated(
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_pipe)
    return;

  DCHECK(data_pipe->socket.is_valid_platform_file());
  base::ScopedPlatformFile socket_handle = data_pipe->socket.TakePlatformFile();
  base::UnsafeSharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  DCHECK(!audio_callback_);
  DCHECK(!audio_thread_);
  audio_callback_ = std::make_unique<media::AudioOutputDeviceThreadCallback>(
      audio_parameters_, std::move(shared_memory_region), render_callback_);
  audio_thread_ = std::make_unique<media::AudioDeviceThread>(
      audio_callback_.get(), std::move(socket_handle), "audio::OutputDevice",
      base::ThreadType::kRealtimeAudio);
}

void OutputDevice::OnConnectionError() {
  // Connection errors should be rare and handling them synchronously is
  // simpler.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  CleanUp();

  render_callback_->OnRenderError();
}

void OutputDevice::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_thread_.reset();  // Blocking call.
  audio_callback_.reset();
  stream_.reset();
  stream_factory_.reset();
}

}  // namespace audio
