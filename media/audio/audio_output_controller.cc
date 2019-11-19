// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_controller.h"

#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_timestamp_helper.h"

using base::TimeDelta;

namespace media {
namespace {

// Time in seconds between two successive measurements of audio power levels.
constexpr int kPowerMonitorLogIntervalSeconds = 15;

// Used to log the result of rendering startup.
// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before
// STREAM_CREATION_RESULT_MAX and update STREAM_CREATION_RESULT_MAX.
enum StreamCreationResult {
  STREAM_CREATION_OK = 0,
  STREAM_CREATION_CREATE_FAILED = 1,
  STREAM_CREATION_OPEN_FAILED = 2,
  STREAM_CREATION_RESULT_MAX = STREAM_CREATION_OPEN_FAILED,
};

void LogStreamCreationResult(bool for_device_change,
                             StreamCreationResult result) {
  if (for_device_change) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.AudioOutputController.ProxyStreamCreationResultForDeviceChange",
        result, STREAM_CREATION_RESULT_MAX + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.AudioOutputController.ProxyStreamCreationResult", result,
        STREAM_CREATION_RESULT_MAX + 1);
  }
}

}  // namespace

AudioOutputController::ErrorStatisticsTracker::ErrorStatisticsTracker()
    : start_time_(base::TimeTicks::Now()), on_more_io_data_called_(0) {
  // WedgeCheck() will look to see if |on_more_io_data_called_| is true after
  // the timeout expires and log this as a UMA stat. If the stream is
  // paused/closed before the timer fires, nothing is logged.
  wedge_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(5), this,
                     &ErrorStatisticsTracker::WedgeCheck);
}

AudioOutputController::ErrorStatisticsTracker::~ErrorStatisticsTracker() {
  UMA_HISTOGRAM_LONG_TIMES("Media.OutputStreamDuration",
                           base::TimeTicks::Now() - start_time_);
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputController.CallbackError",
                        error_during_callback_);
}

void AudioOutputController::ErrorStatisticsTracker::RegisterError() {
  error_during_callback_ = true;
}

void AudioOutputController::ErrorStatisticsTracker::OnMoreDataCalled() {
  // Indicate that we haven't wedged (at least not indefinitely, WedgeCheck()
  // may have already fired if OnMoreData() took an abnormal amount of time).
  // Since this thread is the only writer of |on_more_io_data_called_| once the
  // thread starts, it's safe to compare and then increment.
  if (on_more_io_data_called_.IsZero())
    on_more_io_data_called_.Increment();
}

void AudioOutputController::ErrorStatisticsTracker::WedgeCheck() {
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputControllerPlaybackStartupSuccess",
                        on_more_io_data_called_.IsOne());
}

AudioOutputController::AudioOutputController(
    AudioManager* audio_manager,
    EventHandler* handler,
    const AudioParameters& params,
    const std::string& output_device_id,
    SyncReader* sync_reader)
    : audio_manager_(audio_manager),
      params_(params),
      handler_(handler),
      task_runner_(audio_manager->GetTaskRunner()),
      construction_time_(base::TimeTicks::Now()),
      output_device_id_(output_device_id),
      stream_(NULL),
      diverting_to_stream_(NULL),
      should_duplicate_(0),
      volume_(1.0),
      state_(kEmpty),
      sync_reader_(sync_reader),
      power_monitor_(
          params.sample_rate(),
          TimeDelta::FromMilliseconds(kPowerMeasurementTimeConstantMillis)) {
  DCHECK(audio_manager);
  DCHECK(handler_);
  DCHECK(sync_reader_);
  DCHECK(task_runner_.get());
  weak_this_for_errors_ = weak_factory_for_errors_.GetWeakPtr();
}

AudioOutputController::~AudioOutputController() {
  CHECK_EQ(kClosed, state_);
  CHECK_EQ(nullptr, stream_);
  CHECK(duplication_targets_.empty());
  UMA_HISTOGRAM_LONG_TIMES("Media.AudioOutputController.LifeTime",
                           base::TimeTicks::Now() - construction_time_);
}

// static
scoped_refptr<AudioOutputController> AudioOutputController::Create(
    AudioManager* audio_manager,
    EventHandler* event_handler,
    const AudioParameters& params,
    const std::string& output_device_id,
    const base::UnguessableToken& group_id,
    SyncReader* sync_reader) {
  CHECK(audio_manager);
  CHECK_EQ(AudioManager::Get(), audio_manager);
  DCHECK(sync_reader);
  DCHECK(params.IsValid());

  scoped_refptr<AudioOutputController> controller(new AudioOutputController(
      audio_manager, event_handler, params, output_device_id, sync_reader));

  if (controller->task_runner_->BelongsToCurrentThread()) {
    controller->DoCreate(false);
    audio_manager->AddDiverter(group_id, controller.get());
    return controller;
  }

  controller->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputController::DoCreate, controller, false));
  audio_manager->AddDiverter(group_id, controller.get());
  return controller;
}

void AudioOutputController::Play() {
  CHECK_EQ(AudioManager::Get(), audio_manager_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (task_runner_->BelongsToCurrentThread()) {
    DoPlay();
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AudioOutputController::DoPlay, this));
}

void AudioOutputController::Pause() {
  CHECK_EQ(AudioManager::Get(), audio_manager_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (task_runner_->BelongsToCurrentThread()) {
    DoPause();
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AudioOutputController::DoPause, this));
}

void AudioOutputController::Close(base::OnceClosure closed_task) {
  CHECK_EQ(AudioManager::Get(), audio_manager_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (task_runner_->BelongsToCurrentThread()) {
    DCHECK(closed_task.is_null());
    DoClose();
    audio_manager_->RemoveDiverter(this);
    return;
  }

  DCHECK(!closed_task.is_null());
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&AudioOutputController::DoClose, this),
      base::BindOnce(
          [](scoped_refptr<AudioOutputController> controller,
             base::OnceClosure closed_task) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(controller->owning_sequence_);

            controller->audio_manager_->RemoveDiverter(controller.get());
            controller = nullptr;

            std::move(closed_task).Run();
          },
          base::WrapRefCounted(this), std::move(closed_task)));
}

void AudioOutputController::Flush() {
  CHECK_EQ(AudioManager::Get(), audio_manager_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (task_runner_->BelongsToCurrentThread()) {
    DoFlush();
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AudioOutputController::DoFlush, this));
}

void AudioOutputController::SetVolume(double volume) {
  CHECK_EQ(AudioManager::Get(), audio_manager_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  if (task_runner_->BelongsToCurrentThread()) {
    DoSetVolume(volume);
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputController::DoSetVolume, this, volume));
}

void AudioOutputController::DoCreate(bool is_for_device_change) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.CreateTime");
  TRACE_EVENT0("audio", "AudioOutputController::DoCreate");
  handler_->OnLog(is_for_device_change ? "AOC::DoCreate (for device change)"
                                       : "AOC::DoCreate");

  // Close() can be called before DoCreate() is executed.
  if (state_ == kClosed)
    return;

  DoStopCloseAndClearStream();  // Calls RemoveOutputDeviceChangeListener().
  DCHECK_EQ(kEmpty, state_);

  stream_ = diverting_to_stream_ ?
      diverting_to_stream_ :
      audio_manager_->MakeAudioOutputStreamProxy(params_, output_device_id_);
  if (!stream_) {
    state_ = kError;
    LogStreamCreationResult(is_for_device_change,
                            STREAM_CREATION_CREATE_FAILED);
    handler_->OnControllerError();
    return;
  }

  if (!stream_->Open()) {
    DoStopCloseAndClearStream();
    LogStreamCreationResult(is_for_device_change, STREAM_CREATION_OPEN_FAILED);
    state_ = kError;
    handler_->OnControllerError();
    return;
  }

  LogStreamCreationResult(is_for_device_change, STREAM_CREATION_OK);

  // Everything started okay, so re-register for state change callbacks if
  // stream_ was created via AudioManager.
  if (stream_ != diverting_to_stream_)
    audio_manager_->AddOutputDeviceChangeListener(this);

  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  state_ = kCreated;

  // And then report we have been created if we haven't done so already.
  if (!is_for_device_change)
    handler_->OnControllerCreated();
}

void AudioOutputController::DoPlay() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.PlayTime");
  TRACE_EVENT0("audio", "AudioOutputController::DoPlay");
  handler_->OnLog("AOC::DoPlay");

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;

  // Ask for first packet.
  sync_reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), 0);

  state_ = kPlaying;

  if (will_monitor_audio_levels()) {
    last_audio_level_log_time_ = base::TimeTicks::Now();
  }

  stats_tracker_.emplace();

  stream_->Start(this);

  handler_->OnControllerPlaying();
}

void AudioOutputController::StopStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kPlaying) {
    stream_->Stop();
    stats_tracker_.reset();

    if (will_monitor_audio_levels()) {
      LogAudioPowerLevel("StopStream");
    }

    // A stopped stream is silent, and power_montior_.Scan() is no longer being
    // called; so we must reset the power monitor.
    power_monitor_.Reset();

    state_ = kPaused;
  }
}

void AudioOutputController::DoPause() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.PauseTime");
  TRACE_EVENT0("audio", "AudioOutputController::DoPause");
  handler_->OnLog("AOC::DoPause");

  StopStream();

  if (state_ != kPaused)
    return;

  // Let the renderer know we've stopped.  Necessary to let PPAPI clients know
  // audio has been shutdown.  TODO(dalecurtis): This stinks.  PPAPI should have
  // a better way to know when it should exit PPB_Audio_Shared::Run().
  sync_reader_->RequestMoreData(base::TimeDelta::Max(), base::TimeTicks(), 0);

  handler_->OnControllerPaused();
}

void AudioOutputController::DoClose() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.CloseTime");
  TRACE_EVENT0("audio", "AudioOutputController::DoClose");
  handler_->OnLog("AOC::DoClose");

  if (state_ != kClosed) {
    DoStopCloseAndClearStream();
    sync_reader_->Close();
    state_ = kClosed;
  }
}

void AudioOutputController::DoFlush() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.FlushTime");
  TRACE_EVENT0("audio", "AudioOutputController::DoFlush");
  handler_->OnLog("AOC::DoFlush");

  if (stream_) {
    if (state_ == kPlaying) {
      handler_->OnControllerError();
    } else {
      stream_->Flush();
    }
  }
}

void AudioOutputController::DoSetVolume(double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  switch (state_) {
    case kCreated:
    case kPlaying:
    case kPaused:
      stream_->SetVolume(volume_);
      break;
    default:
      return;
  }
}

void AudioOutputController::DoReportError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "AudioOutputController::DoReportError");
  DLOG(ERROR) << "AudioOutputController::DoReportError";
  if (state_ != kClosed) {
    if (stats_tracker_)
      stats_tracker_->RegisterError();
    handler_->OnControllerError();
  }
}

int AudioOutputController::OnMoreData(base::TimeDelta delay,
                                      base::TimeTicks delay_timestamp,
                                      int prior_frames_skipped,
                                      AudioBus* dest) {
  TRACE_EVENT_BEGIN1("audio", "AudioOutputController::OnMoreData",
                     "frames skipped", prior_frames_skipped);

  stats_tracker_->OnMoreDataCalled();

  sync_reader_->Read(dest);

  const int frames =
      dest->is_bitstream_format() ? dest->GetBitstreamFrames() : dest->frames();
  delay += AudioTimestampHelper::FramesToTime(frames, params_.sample_rate());

  sync_reader_->RequestMoreData(delay, delay_timestamp, prior_frames_skipped);

  if (should_duplicate_.IsOne()) {
    const base::TimeTicks reference_time = delay_timestamp + delay;
    std::unique_ptr<AudioBus> copy(AudioBus::Create(params_));
    dest->CopyTo(copy.get());
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AudioOutputController::BroadcastDataToDuplicationTargets, this,
            std::move(copy), reference_time));
  }

  if (will_monitor_audio_levels()) {
    // Note: this code path should never be hit when using bitstream streams.
    // Scan doesn't expect compressed audio, so it may go out of bounds trying
    // to read |frames| frames of PCM data.
    CHECK(!params_.IsBitstreamFormat());
    power_monitor_.Scan(*dest, frames);

    const auto now = base::TimeTicks::Now();
    if ((now - last_audio_level_log_time_).InSeconds() >
        kPowerMonitorLogIntervalSeconds) {
      LogAudioPowerLevel("OnMoreData");
      last_audio_level_log_time_ = now;
    }
  }

  TRACE_EVENT_END2("audio", "AudioOutputController::OnMoreData",
                   "timestamp (ms)",
                   (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
                   "delay (ms)", delay.InMillisecondsF());
  return frames;
}

void AudioOutputController::BroadcastDataToDuplicationTargets(
    std::unique_ptr<AudioBus> audio_bus,
    base::TimeTicks reference_time) {
  TRACE_EVENT1("audio",
               "AudioOutputController::BroadcastDataToDuplicationTargets",
               "reference_time (ms)",
               (reference_time - base::TimeTicks()).InMillisecondsF());
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != kPlaying || duplication_targets_.empty())
    return;

  // Note: Do not need to acquire lock since this is running on the same thread
  // as where the set is modified.
  for (auto target = std::next(duplication_targets_.begin(), 1);
       target != duplication_targets_.end(); ++target) {
    std::unique_ptr<AudioBus> copy(AudioBus::Create(params_));
    audio_bus->CopyTo(copy.get());
    (*target)->OnData(std::move(copy), reference_time);
  }

  (*duplication_targets_.begin())->OnData(std::move(audio_bus), reference_time);
}

void AudioOutputController::LogAudioPowerLevel(const std::string& call_name) {
  std::pair<float, bool> power_and_clip =
      power_monitor_.ReadCurrentPowerAndClip();
  handler_->OnLog(base::StringPrintf("AOC::%s: average audio level=%.2f dBFS",
                                     call_name.c_str(), power_and_clip.first));
}

void AudioOutputController::OnError() {
  // Handle error on the audio controller thread.  We defer errors for one
  // second in case they are the result of a device change; delay chosen to
  // exceed duration of device changes which take a few hundred milliseconds.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputController::DoReportError,
                     weak_this_for_errors_),
      base::TimeDelta::FromSeconds(1));
}

void AudioOutputController::DoStopCloseAndClearStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream_ to close.
  if (stream_) {
    // Ensure no errors will be delivered while we cycle streams and any that
    // occurred immediately prior to the device change are dropped.
    weak_factory_for_errors_.InvalidateWeakPtrs();

    // De-register from state change callbacks if stream_ was created via
    // AudioManager.
    if (stream_ != diverting_to_stream_)
      audio_manager_->RemoveOutputDeviceChangeListener(this);

    StopStream();
    stream_->Close();
    stats_tracker_.reset();

    if (stream_ == diverting_to_stream_)
      diverting_to_stream_ = NULL;
    stream_ = NULL;

    // Since the stream is stopped, we can now update |weak_this_for_errors_|.
    weak_this_for_errors_ = weak_factory_for_errors_.GetWeakPtr();
  }

  state_ = kEmpty;
}

void AudioOutputController::OnDeviceChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.DeviceChangeTime");
  TRACE_EVENT0("audio", "AudioOutputController::OnDeviceChange");

  auto state_to_string = [](State state) {
    switch (state) {
      case AudioOutputController::kEmpty:
        return "empty";
      case AudioOutputController::kCreated:
        return "created";
      case AudioOutputController::kPlaying:
        return "playing";
      case AudioOutputController::kPaused:
        return "paused";
      case AudioOutputController::kClosed:
        return "closed";
      case AudioOutputController::kError:
        return "error";
    }
    return "unknown";
  };

  handler_->OnLog(base::StringPrintf("AOC::OnDeviceChange while in state: %s",
                                     state_to_string(state_)));

  // TODO(dalecurtis): Notify the renderer side that a device change has
  // occurred.  Currently querying the hardware information here will lead to
  // crashes on OSX.  See http://crbug.com/158170.

  // Recreate the stream (DoCreate() will first shut down an existing stream).
  // Exit if we ran into an error.
  const State original_state = state_;
  DoCreate(true);
  if (!stream_ || state_ == kError)
    return;

  // Get us back to the original state or an equivalent state.
  switch (original_state) {
    case kPlaying:
      DoPlay();
      return;
    case kCreated:
    case kPaused:
      // From the outside these two states are equivalent.
      return;
    default:
      NOTREACHED() << "Invalid original state.";
  }
}

const AudioParameters& AudioOutputController::GetAudioParameters() {
  return params_;
}

void AudioOutputController::StartDiverting(AudioOutputStream* to_stream) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputController::DoStartDiverting, this,
                                to_stream));
}

void AudioOutputController::StopDiverting() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputController::DoStopDiverting, this));
}

void AudioOutputController::StartDuplicating(AudioPushSink* sink) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputController::DoStartDuplicating, this, sink));
}

void AudioOutputController::StopDuplicating(AudioPushSink* sink) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputController::DoStopDuplicating, this, sink));
}

void AudioOutputController::DoStartDiverting(AudioOutputStream* to_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kClosed)
    return;

  DCHECK(!diverting_to_stream_);
  diverting_to_stream_ = to_stream;
  DoStartOrStopDivertingInternal();
}

void AudioOutputController::DoStopDiverting() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kClosed)
    return;

  DoStartOrStopDivertingInternal();
  DCHECK(!diverting_to_stream_);
}

void AudioOutputController::DoStartOrStopDivertingInternal() {
  TRACE_EVENT0("audio",
               "AudioOutputController::DoStartOrStopDivertingInternal");

  handler_->OnLog(base::StringPrintf(
      "AOC::DoStartOrStopDivertingInternal() will %s diverting",
      (stream_ == diverting_to_stream_) ? "stop" : "start"));

  // Re-create the stream: First, shut down an existing stream (if any), then
  // attempt to open either: a) the |diverting_to_stream_|, or b) a normal
  // stream from the AudioManager. If that fails, error-out the controller.
  // Otherwise, set the volume and restore playback if the prior stream was
  // playing.
  const bool restore_playback = (state_ == kPlaying);
  DoStopCloseAndClearStream();  // Calls RemoveOutputDeviceChangeListener().
  DCHECK_EQ(kEmpty, state_);
  stream_ = diverting_to_stream_ ? diverting_to_stream_
                                 : audio_manager_->MakeAudioOutputStreamProxy(
                                       params_, output_device_id_);
  if (!stream_ || !stream_->Open()) {
    DoStopCloseAndClearStream();
    state_ = kError;
    handler_->OnControllerError();
    return;
  }
  if (stream_ != diverting_to_stream_)
    audio_manager_->AddOutputDeviceChangeListener(this);
  stream_->SetVolume(volume_);
  state_ = kCreated;
  if (restore_playback)
    DoPlay();
}

void AudioOutputController::DoStartDuplicating(AudioPushSink* to_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kClosed)
    return;

  if (duplication_targets_.empty())
    should_duplicate_.Increment();

  duplication_targets_.insert(to_stream);
}

void AudioOutputController::DoStopDuplicating(AudioPushSink* to_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  to_stream->Close();

  duplication_targets_.erase(to_stream);
  if (duplication_targets_.empty()) {
    const bool is_nonzero = should_duplicate_.Decrement();
    DCHECK(!is_nonzero);
  }
}

std::pair<float, bool> AudioOutputController::ReadCurrentPowerAndClip() {
  DCHECK(will_monitor_audio_levels());
  return power_monitor_.ReadCurrentPowerAndClip();
}

}  // namespace media
