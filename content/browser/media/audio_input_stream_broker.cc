// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_input_stream_broker.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/audio_stream_broker_helper.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_logging.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "media/mojo/mojom/audio_processing.mojom.h"

namespace content {

using DisconnectReason =
    media::mojom::AudioInputStreamObserver::DisconnectReason;
using InputStreamErrorCode = media::mojom::InputStreamErrorCode;

AudioInputStreamBroker::AudioInputStreamBroker(
    int render_process_id,
    int render_frame_id,
    const std::string& device_id,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    media::UserInputMonitorBase* user_input_monitor,
    bool enable_agc,
    media::mojom::AudioProcessingConfigPtr processing_config,
    AudioStreamBroker::DeleterCallback deleter,
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        renderer_factory_client)
    : AudioStreamBroker(render_process_id, render_frame_id),
      device_id_(device_id),
      params_(params),
      shared_memory_count_(shared_memory_count),
      user_input_monitor_(user_input_monitor),
      enable_agc_(enable_agc),
      deleter_(std::move(deleter)),
      processing_config_(std::move(processing_config)),
      renderer_factory_client_(std::move(renderer_factory_client)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(renderer_factory_client_);
  DCHECK(deleter_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "AudioInputStreamBroker", this);

  // Unretained is safe because |this| owns |renderer_factory_client_|.
  renderer_factory_client_.set_disconnect_handler(base::BindOnce(
      &AudioInputStreamBroker::ClientBindingLost, base::Unretained(this)));

  NotifyFrameHostOfAudioStreamStarted(render_process_id, render_frame_id,
                                      /*is_capturing=*/true);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    params_.set_format(media::AudioParameters::AUDIO_FAKE);
  }
}

AudioInputStreamBroker::~AudioInputStreamBroker() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This relies on CreateStream() being called synchronously right after the
  // constructor.
  if (user_input_monitor_)
    user_input_monitor_->DisableKeyPressMonitoring();

  NotifyFrameHostOfAudioStreamStopped(render_process_id(), render_frame_id(),
                                      /*is_capturing=*/true);

  // TODO(crbug.com/40091014) update tab recording indicator.

  if (awaiting_created_) {
    TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "CreateStream", this, "success",
                                    "failed or cancelled");
  }
  TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "AudioInputStreamBroker", this,
                                  "disconnect reason",
                                  static_cast<uint32_t>(disconnect_reason_));
}

void AudioInputStreamBroker::CreateStream(
    media::mojom::AudioStreamFactory* factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!observer_receiver_.is_bound());
  DCHECK(!pending_client_receiver_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("audio", "CreateStream", this, "device id",
                                    device_id_);
  awaiting_created_ = true;

  base::ReadOnlySharedMemoryRegion key_press_count_buffer;
  if (user_input_monitor_) {
    key_press_count_buffer =
        user_input_monitor_->EnableKeyPressMonitoringWithMapping();
  }

  mojo::PendingRemote<media::mojom::AudioInputStreamClient> client;
  pending_client_receiver_ = client.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<media::mojom::AudioInputStream> stream;
  auto stream_receiver = stream.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer;
  observer_receiver_.Bind(observer.InitWithNewPipeAndPassReceiver());

  // Unretained is safe because |this| owns |observer_binding_|.
  observer_receiver_.set_disconnect_with_reason_handler(base::BindOnce(
      &AudioInputStreamBroker::ObserverBindingLost, base::Unretained(this)));

  // Note that the component id for AudioLog is used to differentiate between
  // several users of the same audio log. Since this audio log is for a single
  // stream, the component id used doesn't matter.
  // We can move |processing_config_| because this method is only called once.
  constexpr int log_component_id = 0;
  factory->CreateInputStream(
      std::move(stream_receiver), std::move(client), std::move(observer),
      MediaInternals::GetInstance()->CreateMojoAudioLog(
          media::AudioLogFactory::AudioComponent::kAudioInputController,
          log_component_id, render_process_id(), render_frame_id()),
      device_id_, params_, shared_memory_count_, enable_agc_,
      std::move(key_press_count_buffer), std::move(processing_config_),
      base::BindOnce(&AudioInputStreamBroker::StreamCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(stream)));
}

void AudioInputStreamBroker::DidStartRecording() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(crbug.com/40091014) update tab recording indicator.
}

void AudioInputStreamBroker::StreamCreated(
    mojo::PendingRemote<media::mojom::AudioInputStream> stream,
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
    bool initially_muted,
    const std::optional<base::UnguessableToken>& stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  awaiting_created_ = false;
  TRACE_EVENT_NESTABLE_ASYNC_END1("audio", "CreateStream", this, "success",
                                  !!data_pipe);

  if (!data_pipe) {
    disconnect_reason_ = DisconnectReason::kStreamCreationFailed;
    Cleanup();
    return;
  }

  DCHECK(stream_id.has_value());
  DCHECK(renderer_factory_client_);
  renderer_factory_client_->StreamCreated(
      std::move(stream), std::move(pending_client_receiver_),
      std::move(data_pipe), initially_muted, stream_id);
}

InputStreamErrorCode MapDisconnectReasonToErrorCode(DisconnectReason reason) {
  switch (static_cast<DisconnectReason>(reason)) {
    case DisconnectReason::kSystemPermissions:
      return InputStreamErrorCode::kSystemPermissions;
    case DisconnectReason::kDeviceInUse:
      return InputStreamErrorCode::kDeviceInUse;
    case DisconnectReason::kDefault:
    case DisconnectReason::kPlatformError:
    case DisconnectReason::kTerminatedByClient:
    case DisconnectReason::kStreamCreationFailed:
    case DisconnectReason::kDocumentDestroyed:
      break;
  }
  return InputStreamErrorCode::kUnknown;
}

void AudioInputStreamBroker::ObserverBindingLost(
    uint32_t reason,
    const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DisconnectReason disconnection_reason = static_cast<DisconnectReason>(reason);
  if (!media::mojom::IsKnownEnumValue(disconnection_reason)) {
    DLOG(ERROR) << "Invalid reason: " << reason;
  } else if (disconnect_reason_ == DisconnectReason::kDocumentDestroyed) {
    disconnect_reason_ = disconnection_reason;
  }

  renderer_factory_client_.ResetWithReason(
      static_cast<uint32_t>(
          MapDisconnectReasonToErrorCode(disconnection_reason)),
      description);

  Cleanup();
}

void AudioInputStreamBroker::ClientBindingLost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  disconnect_reason_ = DisconnectReason::kTerminatedByClient;
  Cleanup();
}

void AudioInputStreamBroker::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::move(deleter_).Run(this);
}

}  // namespace content
