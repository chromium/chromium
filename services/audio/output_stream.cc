// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_stream.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace audio {

std::string GetCtorLogString(media::AudioManager* audio_manager,
                             const std::string& device_id,
                             const media::AudioParameters& params) {
  return base::StringPrintf(
      "Ctor({audio_manager_name=%s}, {device_id=%s}, {params=[%s]})",
      audio_manager->GetName(), device_id.c_str(),
      params.AsHumanReadableString().c_str());
}

class AudibilityHelperImpl : public OutputStream::AudibilityHelper {
 public:
  // Threshold in dbfs at which we consider audio output levels to be
  // inaudible/silent. There was is no documentation outlining how this
  // value was initially chosen, but it has been in use for years now.
  static constexpr float kSilenceThresholdDBFS = -72.24719896f;

  // The minimum amount of time audio must be below kSilenceThresholdDBFS for
  // the OutputStream to report silence.
  // Chosen such that it won't meaningfully change the "this tab is playing
  // audio" icon's responsiveness, while still preventing state changes when
  // there is a glitch (assuming a 20ms buffer size).
  static constexpr base::TimeDelta kGlitchTolerance = base::Milliseconds(100);

  // Desired polling frequency.  Note: If this is set too low, short-duration
  // "blip" sounds won't be detected.  http://crbug.com/339133#c4
  static constexpr base::TimeDelta kPollingPeriod = base::Seconds(1) / 15;

  // Special value for `first_time_silent_`, to bypass the grace period. This
  // ensures that *silent* streams that were just created or unpaused are above
  // the `kGlitchTolerance` threshold.
  static constexpr base::TimeTicks kForcedSilenceStartTime =
      base::TimeTicks::Min();

  AudibilityHelperImpl() = default;
  ~AudibilityHelperImpl() override = default;

  void StartPolling(
      GetPowerLevelCB get_power_level_cb,
      OnAudibleStateChangedCB on_audible_state_changed_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

    CHECK(!get_power_level_cb_ && get_power_level_cb);
    CHECK(!on_audible_state_changed_cb_ && on_audible_state_changed_cb);
    get_power_level_cb_ = std::move(get_power_level_cb);
    on_audible_state_changed_cb_ = std::move(on_audible_state_changed_cb);

    CHECK(!poll_timer_.IsRunning());

    // Bypass the grace period when starting a stream.
    // The first time PollAudioLevel() is called we will immediately fall into
    // the correct state:
    // - Silent streams will remain silent, without reporting audibility for a
    //   period of `kGlitchTolerance`.
    // - Audible streams will immediately report audibility.
    CHECK(!is_audible_);
    first_time_silent_ = kForcedSilenceStartTime;

    // base::Unretained is safe because |this| owns |poll_timer_|.
    poll_timer_.Start(FROM_HERE, kPollingPeriod,
                      base::BindRepeating(&AudibilityHelperImpl::PollAudioLevel,
                                          base::Unretained(this)));
  }

  void StopPolling() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

    poll_timer_.Stop();

    // Stopped streams are silent.
    if (is_audible_) {
      is_audible_ = false;
      on_audible_state_changed_cb_.Run(false);
    }

    get_power_level_cb_.Reset();
    on_audible_state_changed_cb_.Reset();
  }

  bool IsAudible() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    return is_audible_;
  }

 private:
  void PollAudioLevel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

    bool was_audible = is_audible_;
    is_audible_ = ComputeIsAudible();

    if (is_audible_ != was_audible) {
      on_audible_state_changed_cb_.Run(is_audible_);
    }
  }

  bool ComputeIsAudible() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

    const float power_dbfs = get_power_level_cb_.Run();
    const bool currently_audible = power_dbfs >= kSilenceThresholdDBFS;

    const base::TimeTicks now = base::TimeTicks::Now();

    // Always instantly report audible signals.
    if (currently_audible) {
      first_time_silent_ = std::nullopt;
      return true;
    }

    if (!first_time_silent_) {
      // The stream just became silent.
      first_time_silent_ = now;
    }

    const base::TimeDelta silence_duration = now - first_time_silent_.value();

    // Only report silence after a small grace period. This prevents small
    // audio glitches from changing the audibility state, and potentially
    // downgrading the tab's priority.
    return silence_duration <= kGlitchTolerance;
  }

  SEQUENCE_CHECKER(owning_sequence_);

  GetPowerLevelCB get_power_level_cb_;
  OnAudibleStateChangedCB on_audible_state_changed_cb_;

  // Calls PollAudioLevel() at regular intervals while |playing_| is true.
  base::RepeatingTimer poll_timer_;

  // The time at which the stream last transitioned to silence.
  std::optional<base::TimeTicks> first_time_silent_;

  // Streams start as silent, without a grace period. This is because the
  // silent -> audible transition is instant (and we will be in the correct
  // state after a single call to PollAudioLevels()). This also prevents silent
  // streams from causing a "this tab is playing audio" icon to appear, when
  // no audio was emitted.
  bool is_audible_ = false;
};

OutputStream::OutputStream(
    CreatedCallback created_callback,
    DeleteCallback delete_callback,
    ManagedDeviceOutputStreamCreateCallback
        managed_device_output_stream_create_callback,
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
    mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
        device_switch_receiver,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    media::AudioManager* audio_manager,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& loopback_group_id)
    : delete_callback_(std::move(delete_callback)),
      receiver_(this, std::move(stream_receiver)),
      device_switch_receiver_(this, std::move(device_switch_receiver)),
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
                  std::move(managed_device_output_stream_create_callback)),
      loopback_group_id_(loopback_group_id),
      audibility_helper_(std::make_unique<AudibilityHelperImpl>()) {
  DCHECK(receiver_.is_bound());
  DCHECK(created_callback);
  DCHECK(delete_callback_);
  DCHECK(coordinator_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "audio::OutputStream", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("audio", "OutputStream", this, "device id",
                                    output_device_id, "params",
                                    params.AsHumanReadableString());
  SendLogMessage(
      "%s", GetCtorLogString(audio_manager, output_device_id, params).c_str());

  // |this| owns these objects, so unretained is safe.
  base::RepeatingClosure error_handler =
      base::BindRepeating(&OutputStream::OnError, base::Unretained(this));
  receiver_.set_disconnect_handler(error_handler);
  if (device_switch_receiver_.is_bound()) {
    device_switch_receiver_.set_disconnect_handler(error_handler);
  }

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

  if (log_) {
    log_->OnClosed();
  }

  if (observer_) {
    observer_.ResetWithReason(
        static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                                  DisconnectReason::kTerminatedByClient),
        std::string());
  }

  controller_.Close();
  coordinator_->UnregisterMember(loopback_group_id_, &controller_);

  if (audibility_helper_->IsAudible()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Audible", this);
  }

  if (playing_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Playing", this);
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "OutputStream", this);
  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "audio::OutputStream", this);
}

void OutputStream::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage("%s()", __func__);

  controller_.Play();
  if (log_)
    log_->OnStarted();
}

void OutputStream::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage("%s()", __func__);

  controller_.Pause();
  if (log_)
    log_->OnStopped();
}

void OutputStream::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage("%s()", __func__);

  controller_.Flush();
}

void OutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("audio", "SetVolume", this, "volume",
                                      volume);

  if (volume < 0 || volume > 1) {
    receiver_.ReportBadMessage("Invalid volume");
    OnControllerError();
    return;
  }

  controller_.SetVolume(volume);
  if (log_)
    log_->OnSetVolume(volume);
}

void OutputStream::SwitchAudioOutputDeviceId(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("audio", "SwitchAudioOutputDeviceId",
                                      this, "device_id", output_device_id);

  controller_.SwitchAudioOutputDeviceId(output_device_id);
}

void OutputStream::CreateAudioPipe(CreatedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(reader_.IsValid());
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "CreateAudioPipe", this);
  SendLogMessage("%s()", __func__);

  base::UnsafeSharedMemoryRegion shared_memory_region =
      reader_.TakeSharedMemoryRegion();
  mojo::PlatformHandle socket_handle(foreign_socket_.Take());
  if (!shared_memory_region.IsValid() || !socket_handle.is_valid()) {
    std::move(created_callback).Run(nullptr);
    OnError();
    return;
  }

  std::move(created_callback)
      .Run({std::in_place, std::move(shared_memory_region),
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
    const auto get_power_level = [](OutputStream* self) {
      return self->controller_.ReadCurrentPowerAndClip().first;
    };

    audibility_helper_->StartPolling(
        base::BindRepeating(std::move(get_power_level), base::Unretained(this)),
        base::BindRepeating(&OutputStream::OnAudibleStateChanged,
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
    audibility_helper_->StopPolling();
  }
  if (observer_)
    observer_->DidStopPlaying();
  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Playing", this);
}

void OutputStream::OnControllerError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "OnControllerError", this);
  SendLogMessage("%s()", __func__);

  // Stop checking the audio level to avoid using this object while it's being
  // torn down.
  audibility_helper_->StopPolling();

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

void OutputStream::OnLog(std::string_view message) {
  // No sequence check: |log_| is thread-safe.
  if (log_) {
    log_->OnLogMessage(base::StringPrintf("%s", std::string(message).c_str()));
  }
}

void OutputStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "OnError", this);

  // Defer callback so we're not destructed while in the constructor.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OutputStream::CallDeleter, weak_factory_.GetWeakPtr()));

  // Ignore any incoming calls.
  receiver_.reset();
}

void OutputStream::CallDeleter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  std::move(delete_callback_).Run(this);
}

// TODO(crbug.com/40104418): it might be useful to track these transitions with
// logs as well but note that the method is called at a rather high rate.
void OutputStream::OnAudibleStateChanged(bool is_audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (is_audible) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "Audible", this);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Audible", this);
  }

  if (observer_) {
    observer_->DidChangeAudibleState(is_audible);
  }
}

void OutputStream::SendLogMessage(const char* format, ...) {
  if (!log_)
    return;
  va_list args;
  va_start(args, format);
  log_->OnLogMessage(
      "audio::OS::" + base::StringPrintV(format, args) +
      base::StringPrintf(" [controller=0x%" PRIXPTR "]",
                         reinterpret_cast<uintptr_t>(&controller_)));
  va_end(args);
}

// Static
std::unique_ptr<OutputStream::AudibilityHelper>
OutputStream::MakeAudibilityHelperForTest() {
  return std::make_unique<AudibilityHelperImpl>();
}

}  // namespace audio
