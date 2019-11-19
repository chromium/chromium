// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/output_device.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/threading/thread_restrictions.h"
#include "media/audio/audio_output_device_thread_callback.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace audio {

OutputDevice::OutputDevice(
    mojo::PendingRemote<mojom::StreamFactory> stream_factory,
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
      base::nullopt,
      base::BindOnce(&OutputDevice::StreamCreated, weak_factory_.GetWeakPtr()));
  stream_.set_disconnect_handler(base::BindOnce(
      &OutputDevice::OnConnectionError, weak_factory_.GetWeakPtr()));
}

OutputDevice::~OutputDevice() {
  CleanUp();
}

void OutputDevice::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Play();
}

void OutputDevice::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->Pause();
}

void OutputDevice::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetVolume(volume);
}

void OutputDevice::StreamCreated(
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_pipe)
    return;

  base::PlatformFile socket_handle;
  auto result =
      mojo::UnwrapPlatformFile(std::move(data_pipe->socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  base::UnsafeSharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  DCHECK(!audio_callback_);
  DCHECK(!audio_thread_);
  audio_callback_ = std::make_unique<media::AudioOutputDeviceThreadCallback>(
      audio_parameters_, std::move(shared_memory_region), render_callback_);
  audio_thread_ = std::make_unique<media::AudioDeviceThread>(
      audio_callback_.get(), socket_handle, "audio::OutputDevice",
      base::ThreadPriority::REALTIME_AUDIO);
}

void OutputDevice::OnConnectionError() {
  // Connection errors should be rare and handling them synchronously is
  // simpler.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  CleanUp();
}

void OutputDevice::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_thread_.reset();  // Blocking call.
  audio_callback_.reset();
  stream_.reset();
  stream_factory_.reset();
}

}  // namespace audio
