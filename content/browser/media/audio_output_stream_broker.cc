// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_output_stream_broker.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/common/content_client.h"
#include "media/audio/audio_logging.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"

namespace content {

namespace {

// Used in Media.Audio.Render.StreamBrokerDisconnectReason2 histogram, matches
// StreamBrokerDisconnectReason2 enum.
enum class StreamBrokerDisconnectReason {
  kDefault = 0,
  kPlatformError,
  kTerminatedByClient,
  kTerminatedByClientAwaitingCreated,
  kStreamCreationFailed,
  kDocumentDestroyed,
  kDocumentDestroyedAwaitingCreated,
  kMaxValue = kDocumentDestroyedAwaitingCreated
};

using DisconnectReason =
    media::mojom::AudioOutputStreamObserver::DisconnectReason;

StreamBrokerDisconnectReason GetDisconnectReason(DisconnectReason reason,
                                                 bool awaiting_created) {
  switch (reason) {
    case DisconnectReason::kPlatformError:
      return StreamBrokerDisconnectReason::kPlatformError;
    case DisconnectReason::kTerminatedByClient:
      return awaiting_created
                 ? StreamBrokerDisconnectReason::
                       kTerminatedByClientAwaitingCreated
                 : StreamBrokerDisconnectReason::kTerminatedByClient;
    case DisconnectReason::kStreamCreationFailed:
      return StreamBrokerDisconnectReason::kStreamCreationFailed;
    case DisconnectReason::kDocumentDestroyed:
      return awaiting_created
                 ? StreamBrokerDisconnectReason::
                       kDocumentDestroyedAwaitingCreated
                 : StreamBrokerDisconnectReason::kDocumentDestroyed;
    case DisconnectReason::kDefault:
      return StreamBrokerDisconnectReason::kDefault;
  }
}

}  // namespace

AudioOutputStreamBroker::AudioOutputStreamBroker(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    DeleterCallback deleter,
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client)
    : AudioStreamBroker(render_process_id, render_frame_id),
      output_device_id_(output_device_id),
      params_(params),
      group_id_(group_id),
      deleter_(std::move(deleter)),
      client_(std::move(client)),
      observer_(render_process_id, render_frame_id, stream_id),
      observer_receiver_(&observer_) {
  DCHECK(client_);
  DCHECK(deleter_);
  DCHECK(group_id_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "AudioOutputStreamBroker", this);

  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  // May be null in unit tests.
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id, render_frame_id);

  // Unretained is safe because |this| owns |client_|
  client_.set_disconnect_handler(
      base::BindOnce(&AudioOutputStreamBroker::Cleanup, base::Unretained(this),
                     DisconnectReason::kTerminatedByClient));
}

AudioOutputStreamBroker::~AudioOutputStreamBroker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  const StreamBrokerDisconnectReason reason =
      GetDisconnectReason(disconnect_reason_, AwaitingCreated());

  if (AwaitingCreated()) {
    TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "CreateStream", this, "success",
                                    "failed or cancelled");
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "AudioOutputStreamBroker", this,
                                  "disconnect reason",
                                  static_cast<uint32_t>(reason));
}

void AudioOutputStreamBroker::CreateStream(
    audio::mojom::StreamFactory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!observer_receiver_.is_bound());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("audio", "CreateStream", this, "device id",
                                    output_device_id_);
  stream_creation_start_time_ = base::TimeTicks::Now();

  // Set up observer ptr. Unretained is safe because |this| owns
  // |observer_receiver_|.
  mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
      observer;
  observer_receiver_.Bind(observer.InitWithNewEndpointAndPassReceiver());
  observer_receiver_.set_disconnect_with_reason_handler(base::BindOnce(
      &AudioOutputStreamBroker::ObserverBindingLost, base::Unretained(this)));

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  auto stream_receiver = stream.InitWithNewPipeAndPassReceiver();

  // Note that the component id for AudioLog is used to differentiate between
  // several users of the same audio log. Since this audio log is for a single
  // stream, the component id used doesn't matter.
  constexpr int log_component_id = 0;
  factory->CreateOutputStream(
      std::move(stream_receiver), std::move(observer),
      MediaInternals::GetInstance()->CreateMojoAudioLog(
          media::AudioLogFactory::AudioComponent::AUDIO_OUTPUT_CONTROLLER,
          log_component_id, render_process_id(), render_frame_id()),
      output_device_id_, params_, group_id_,
      base::BindOnce(&AudioOutputStreamBroker::StreamCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(stream)));
}

void AudioOutputStreamBroker::StreamCreated(
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "CreateStream", this, "success",
                                  !!data_pipe);
  stream_creation_start_time_ = base::TimeTicks();

  if (!data_pipe) {
    // Stream creation failed. Signal error.
    client_.ResetWithReason(
        static_cast<uint32_t>(DisconnectReason::kPlatformError), std::string());
    Cleanup(DisconnectReason::kStreamCreationFailed);
    return;
  }

  client_->Created(std::move(stream), std::move(data_pipe));
}

void AudioOutputStreamBroker::ObserverBindingLost(
    uint32_t reason,
    const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("audio", "ObserverBindingLost", this,
                                      "reset reason", reason);
  if (reason > static_cast<uint32_t>(DisconnectReason::kMaxValue))
    NOTREACHED() << "Invalid reason: " << reason;

  DisconnectReason reason_enum = static_cast<DisconnectReason>(reason);

  // TODO(https://crbug.com/787806): Don't propagate errors if we can retry
  // instead.
  client_.ResetWithReason(
      static_cast<uint32_t>(DisconnectReason::kPlatformError), std::string());
  Cleanup((reason_enum == DisconnectReason::kPlatformError && AwaitingCreated())
              ? DisconnectReason::kStreamCreationFailed
              : reason_enum);
}

void AudioOutputStreamBroker::Cleanup(DisconnectReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK_EQ(DisconnectReason::kDocumentDestroyed, disconnect_reason_);
  disconnect_reason_ = reason;
  std::move(deleter_).Run(this);
}

bool AudioOutputStreamBroker::AwaitingCreated() const {
  return stream_creation_start_time_ != base::TimeTicks();
}

}  // namespace content
