// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/mojo_audio_input_ipc.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/common/input_error_code_converter.h"
#include "media/mojo/mojom/audio_data_pipe.mojom-blink.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace blink {

MojoAudioInputIPC::MojoAudioInputIPC(
    const media::AudioSourceParameters& source_params,
    StreamCreatorCB stream_creator,
    StreamAssociatorCB stream_associator)
    : source_params_(source_params),
      stream_creator_(std::move(stream_creator)),
      stream_associator_(std::move(stream_associator)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(stream_creator_);
  DCHECK(stream_associator_);
}

MojoAudioInputIPC::~MojoAudioInputIPC() = default;

void MojoAudioInputIPC::CreateStream(media::AudioInputIPCDelegate* delegate,
                                     const media::AudioParameters& params,
                                     bool automatic_gain_control,
                                     uint32_t total_segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  DCHECK(!delegate_);

  delegate_ = delegate;

  mojo::PendingRemote<mojom::blink::RendererAudioInputStreamFactoryClient>
      client;
  factory_client_receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
  factory_client_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&MojoAudioInputIPC::OnDisconnect, base::Unretained(this)));

  mojo::PendingReceiver<media::mojom::blink::AudioProcessorControls>
      controls_receiver;

  if (source_params_.processing.has_value())
    controls_receiver = processor_controls_.BindNewPipeAndPassReceiver();

  stream_creator_.Run(source_params_, std::move(client),
                      std::move(controls_receiver), params,
                      automatic_gain_control, total_segments);
}

void MojoAudioInputIPC::RecordStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->Record();
}

void MojoAudioInputIPC::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->SetVolume(volume);
}

void MojoAudioInputIPC::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_) << "Can only be called after the stream has been created";
  // Loopback streams have no stream ids and cannot be use echo cancellation
  if (stream_id_.has_value())
    stream_associator_.Run(*stream_id_, output_device_id);
}

media::AudioProcessorControls* MojoAudioInputIPC::GetProcessorControls() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

void MojoAudioInputIPC::CloseStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = nullptr;
  factory_client_receiver_.reset();
  stream_client_receiver_.reset();
  stream_.reset();
  processor_controls_.reset();
}

void MojoAudioInputIPC::GetStats(GetStatsCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (processor_controls_)
    processor_controls_->GetStats(std::move(callback));
}

void MojoAudioInputIPC::SetPreferredNumCaptureChannels(
    int32_t num_preferred_channels) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (processor_controls_)
    processor_controls_->SetPreferredNumCaptureChannels(num_preferred_channels);
}

void MojoAudioInputIPC::StreamCreated(
    mojo::PendingRemote<media::mojom::blink::AudioInputStream> stream,
    mojo::PendingReceiver<media::mojom::blink::AudioInputStreamClient>
        stream_client_receiver,
    media::mojom::blink::ReadOnlyAudioDataPipePtr data_pipe,
    bool initially_muted,
    const std::optional<base::UnguessableToken>& stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(!stream_);
  DCHECK(!stream_client_receiver_.is_bound());

  stream_.Bind(std::move(stream));
  stream_client_receiver_.Bind(std::move(stream_client_receiver));

  // Keep the stream_id, if we get one. Regular input stream have stream ids,
  // but Loopback streams do not.
  stream_id_ = stream_id;

  DCHECK(data_pipe->socket.is_valid_platform_file());
  base::ScopedPlatformFile socket_handle = data_pipe->socket.TakePlatformFile();

  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  delegate_->OnStreamCreated(std::move(shared_memory_region),
                             std::move(socket_handle), initially_muted);
}

void MojoAudioInputIPC::OnDisconnect(uint32_t error,
                                     const std::string& reason) {
  this->OnError(static_cast<media::mojom::InputStreamErrorCode>(error));
}

void MojoAudioInputIPC::OnError(media::mojom::InputStreamErrorCode code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);

  delegate_->OnError(media::ConvertToCaptureCallbackCode(code));
}

void MojoAudioInputIPC::OnMutedStateChanged(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnMuted(is_muted);
}

}  // namespace blink
