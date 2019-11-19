// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

namespace audio {

const float kSilenceThresholdDBFS = -72.24719896f;

// Desired polling frequency.  Note: If this is set too low, short-duration
// "blip" sounds won't be detected.  http://crbug.com/339133#c4
const int kPowerMeasurementsPerSecond = 15;

OutputStream::OutputStream(
    CreatedCallback created_callback,
    DeleteCallback delete_callback,
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    media::AudioManager* audio_manager,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& loopback_group_id,
    StreamMonitorCoordinator* stream_monitor_coordinator,
    const base::UnguessableToken& processing_id)
    : foreign_socket_(),
      delete_callback_(std::move(delete_callback)),
      receiver_(this, std::move(stream_receiver)),
      observer_(std::move(observer)),
      log_(std::move(log)),
      coordinator_(coordinator),
      // Unretained is safe since we own |reader_|
      reader_(log_ ? base::BindRepeating(&media::mojom::AudioLog::OnLogMessage,
                                         base::Unretained(log_.get()))
                   : base::DoNothing(),
              params,
              &foreign_socket_),
      controller_(audio_manager,
                  this,
                  params,
                  output_device_id,
                  &reader_,
                  stream_monitor_coordinator,
                  processing_id),
      loopback_group_id_(loopback_group_id) {
  DCHECK(receiver_.is_bound());
  DCHECK(created_callback);
  DCHECK(delete_callback_);
  DCHECK(coordinator_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "audio::OutputStream", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("audio", "OutputStream", this, "device id",
                                    output_device_id, "params",
                                    params.AsHumanReadableString());

  // |this| owns these objects, so unretained is safe.
  base::RepeatingClosure error_handler =
      base::BindRepeating(&OutputStream::OnError, base::Unretained(this));
  receiver_.set_disconnect_handler(error_handler);

  // We allow the observer to terminate the stream by closing the message pipe.
  if (observer_)
    observer_.set_disconnect_handler(std::move(error_handler));

  if (log_)
    log_->OnCreated(params, output_device_id);

  coordinator_->RegisterMember(loopback_group_id_, &controller_);
  if (!reader_.IsValid() || !controller_.CreateStream()) {
    // Either SyncReader initialization failed or the controller failed to
    // create the stream. In the latter case, the controller will have called
    // OnControllerError().
    std::move(created_callback).Run(nullptr);
    return;
  }

  CreateAudioPipe(std::move(created_callback));
}

OutputStream::~OutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (log_)
    log_->OnClosed();

  if (observer_) {
    observer_.ResetWithReason(
        static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                                  DisconnectReason::kTerminatedByClient),
        std::string());
  }

  controller_.Close();
  coordinator_->UnregisterMember(loopback_group_id_, &controller_);

  if (is_audible_)
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Audible", this);

  if (playing_)
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Playing", this);

  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "OutputStream", this);
  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "audio::OutputStream", this);
}

void OutputStream::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_.Play();
  if (log_)
    log_->OnStarted();
}

void OutputStream::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_.Pause();
  if (log_)
    log_->OnStopped();
}

void OutputStream::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  controller_.Flush();
}

void OutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("audio", "SetVolume", this, "volume",
                                      volume);

  if (volume < 0 || volume > 1) {
    mojo::ReportBadMessage("Invalid volume");
    OnControllerError();
    return;
  }

  controller_.SetVolume(volume);
  if (log_)
    log_->OnSetVolume(volume);
}

void OutputStream::CreateAudioPipe(CreatedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(reader_.IsValid());
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "CreateAudioPipe", this);

  base::UnsafeSharedMemoryRegion shared_memory_region =
      reader_.TakeSharedMemoryRegion();
  mojo::ScopedHandle socket_handle =
      mojo::WrapPlatformFile(foreign_socket_.Release());
  if (!shared_memory_region.IsValid() || !socket_handle.is_valid()) {
    std::move(created_callback).Run(nullptr);
    OnError();
    return;
  }

  std::move(created_callback)
      .Run({base::in_place, std::move(shared_memory_region),
            std::move(socket_handle)});
}

void OutputStream::OnControllerPlaying() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (playing_)
    return;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "Playing", this);
  playing_ = true;
  if (observer_)
    observer_->DidStartPlaying();
  if (OutputController::will_monitor_audio_levels()) {
    DCHECK(!poll_timer_.IsRunning());
    // base::Unretained is safe because |this| owns |poll_timer_|.
    poll_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(1) / kPowerMeasurementsPerSecond,
        base::BindRepeating(&OutputStream::PollAudioLevel,
                            base::Unretained(this)));
    return;
  }

  // In case we don't monitor audio levels, we assume a stream is audible when
  // it's playing.
  if (observer_)
    observer_->DidChangeAudibleState(true);
}

void OutputStream::OnControllerPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (!playing_)
    return;

  playing_ = false;
  if (OutputController::will_monitor_audio_levels()) {
    DCHECK(poll_timer_.IsRunning());
    poll_timer_.Stop();
  }
  if (observer_)
    observer_->DidStopPlaying();
  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Playing", this);
}

void OutputStream::OnControllerError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "OnControllerError", this);

  // Stop checking the audio level to avoid using this object while it's being
  // torn down.
  poll_timer_.Stop();

  if (log_)
    log_->OnError();

  if (observer_) {
    observer_.ResetWithReason(
        static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                                  DisconnectReason::kPlatformError),
        std::string());
  }

  OnError();
}

void OutputStream::OnLog(base::StringPiece message) {
  // No sequence check: |log_| is thread-safe.
  if (log_)
    log_->OnLogMessage(message.as_string());
}

void OutputStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "OnError", this);

  // Defer callback so we're not destructed while in the constructor.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OutputStream::CallDeleter, weak_factory_.GetWeakPtr()));

  // Ignore any incoming calls.
  receiver_.reset();
}

void OutputStream::CallDeleter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  std::move(delete_callback_).Run(this);
}

void OutputStream::PollAudioLevel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  bool was_audible = is_audible_;
  is_audible_ = IsAudible();

  if (is_audible_ && !was_audible) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "Audible", this);
    if (observer_)
      observer_->DidChangeAudibleState(is_audible_);
  } else if (!is_audible_ && was_audible) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Audible", this);
    if (observer_)
      observer_->DidChangeAudibleState(is_audible_);
  }
}

bool OutputStream::IsAudible() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  float power_dbfs = controller_.ReadCurrentPowerAndClip().first;
  return power_dbfs >= kSilenceThresholdDBFS;
}

}  // namespace audio
