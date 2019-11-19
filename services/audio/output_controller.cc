// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_controller.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_timestamp_helper.h"
#include "services/audio/stream_monitor.h"

using base::TimeDelta;

namespace audio {

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

void LogStreamCreationForDeviceChangeResult(StreamCreationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AudioOutputController.ProxyStreamCreationResultForDeviceChange",
      result, STREAM_CREATION_RESULT_MAX + 1);
}

void LogInitialStreamCreationResult(StreamCreationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AudioOutputController.ProxyStreamCreationResult", result,
      STREAM_CREATION_RESULT_MAX + 1);
}

}  // namespace

OutputController::ErrorStatisticsTracker::ErrorStatisticsTracker()
    : start_time_(base::TimeTicks::Now()), on_more_io_data_called_(0) {
  // WedgeCheck() will look to see if |on_more_io_data_called_| is true after
  // the timeout expires and log this as a UMA stat. If the stream is
  // paused/closed before the timer fires, nothing is logged.
  wedge_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(5), this,
                     &ErrorStatisticsTracker::WedgeCheck);
}

OutputController::ErrorStatisticsTracker::~ErrorStatisticsTracker() {
  UMA_HISTOGRAM_LONG_TIMES("Media.OutputStreamDuration",
                           base::TimeTicks::Now() - start_time_);
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputController.CallbackError",
                        error_during_callback_);
}

void OutputController::ErrorStatisticsTracker::RegisterError() {
  error_during_callback_ = true;
}

void OutputController::ErrorStatisticsTracker::OnMoreDataCalled() {
  // Indicate that we haven't wedged (at least not indefinitely, WedgeCheck()
  // may have already fired if OnMoreData() took an abnormal amount of time).
  // Since this thread is the only writer of |on_more_io_data_called_| once the
  // thread starts, it's safe to compare and then increment.
  if (on_more_io_data_called_.IsZero())
    on_more_io_data_called_.Increment();
}

void OutputController::ErrorStatisticsTracker::WedgeCheck() {
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputControllerPlaybackStartupSuccess",
                        on_more_io_data_called_.IsOne());
}

OutputController::OutputController(
    media::AudioManager* audio_manager,
    EventHandler* handler,
    const media::AudioParameters& params,
    const std::string& output_device_id,
    SyncReader* sync_reader,
    StreamMonitorCoordinator* stream_monitor_coordinator,
    const base::UnguessableToken& processing_id)
    : audio_manager_(audio_manager),
      params_(params),
      handler_(handler),
      task_runner_(audio_manager->GetTaskRunner()),
      construction_time_(base::TimeTicks::Now()),
      output_device_id_(output_device_id),
      stream_(NULL),
      disable_local_output_(false),
      volume_(1.0),
      state_(kEmpty),
      sync_reader_(sync_reader),
      stream_monitor_coordinator_(stream_monitor_coordinator),
      processing_id_(processing_id),
      power_monitor_(
          params.sample_rate(),
          TimeDelta::FromMilliseconds(kPowerMeasurementTimeConstantMillis)) {
  DCHECK(audio_manager);
  DCHECK(handler_);
  DCHECK(sync_reader_);
  DCHECK(task_runner_.get());
  DCHECK(stream_monitor_coordinator_ || processing_id.is_empty());
}

OutputController::~OutputController() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(kClosed, state_);
  DCHECK_EQ(nullptr, stream_);
  DCHECK(snoopers_.empty());
  UMA_HISTOGRAM_LONG_TIMES("Media.AudioOutputController.LifeTime",
                           base::TimeTicks::Now() - construction_time_);
}

bool OutputController::CreateStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  RecreateStreamWithTimingUMA(RecreateReason::INITIAL_STREAM);
  return state_ == kCreated;
}

void OutputController::RecreateStreamWithTimingUMA(
    OutputController::RecreateReason reason) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.CreateTime");
  RecreateStream(reason);
}

void OutputController::RecreateStream(OutputController::RecreateReason reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("audio", "OutputController::RecreateStream", "reason",
               static_cast<int>(reason));

  switch (reason) {
    case RecreateReason::INITIAL_STREAM:
      handler_->OnLog("OutputController::RecreateStream(initial stream)");
      break;
    case RecreateReason::DEVICE_CHANGE:
      handler_->OnLog("OutputController::RecreateStream(device change)");
      break;
    case RecreateReason::LOCAL_OUTPUT_TOGGLE:
      handler_->OnLog("OutputController::RecreateStream(local output toggle)");
      break;
  }

  // Close() can be called before Create() is executed.
  if (state_ == kClosed)
    return;

  StopCloseAndClearStream();  // Calls RemoveOutputDeviceChangeListener().
  DCHECK_EQ(kEmpty, state_);

  if (disable_local_output_) {
    // Create a fake AudioOutputStream that will continue pumping the audio
    // data, but does not play it out anywhere. Pumping the audio data is
    // necessary because video playback is synchronized to the audio stream and
    // will freeze otherwise.
    media::AudioParameters mute_params = params_;
    mute_params.set_format(media::AudioParameters::AUDIO_FAKE);
    stream_ = audio_manager_->MakeAudioOutputStream(
        mute_params, std::string(),
        /*log_callback, not used*/ base::DoNothing());
  } else {
    stream_ =
        audio_manager_->MakeAudioOutputStreamProxy(params_, output_device_id_);
  }

  if (!stream_) {
    state_ = kError;
    // TODO(crbug.com/896484): Results should be counted iff the |stream_| is
    // not a fake one. The |reason| for a non-fake stream to be created doesn't
    // matter, right?
    switch (reason) {
      case RecreateReason::INITIAL_STREAM:
        LogInitialStreamCreationResult(STREAM_CREATION_CREATE_FAILED);
        break;
      case RecreateReason::DEVICE_CHANGE:
        LogStreamCreationForDeviceChangeResult(STREAM_CREATION_CREATE_FAILED);
        break;
      case RecreateReason::LOCAL_OUTPUT_TOGGLE:
        break;  // Not counted in UMAs.
    }
    handler_->OnControllerError();
    return;
  }

  weak_this_for_stream_ = weak_factory_for_stream_.GetWeakPtr();
  if (!stream_->Open()) {
    StopCloseAndClearStream();
    // TODO(crbug.com/896484): Here too.
    switch (reason) {
      case RecreateReason::INITIAL_STREAM:
        LogInitialStreamCreationResult(STREAM_CREATION_OPEN_FAILED);
        break;
      case RecreateReason::DEVICE_CHANGE:
        LogStreamCreationForDeviceChangeResult(STREAM_CREATION_OPEN_FAILED);
        break;
      case RecreateReason::LOCAL_OUTPUT_TOGGLE:
        break;  // Not counted in UMAs.
    }
    state_ = kError;
    handler_->OnControllerError();
    return;
  }

  // TODO(crbug.com/896484): Here three.
  switch (reason) {
    case RecreateReason::INITIAL_STREAM:
      LogInitialStreamCreationResult(STREAM_CREATION_OK);
      break;
    case RecreateReason::DEVICE_CHANGE:
      LogStreamCreationForDeviceChangeResult(STREAM_CREATION_OK);
      break;
    case RecreateReason::LOCAL_OUTPUT_TOGGLE:
      break;  // Not counted in UMAs.
  }

  audio_manager_->AddOutputDeviceChangeListener(this);

  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  state_ = kCreated;

  if (processing_id_) {
    // Ensure new monitors know that we're active.
    stream_monitor_coordinator_->AddObserver(processing_id_, this);
    // Ensure existing monitors do as well.
    stream_monitor_coordinator_->ForEachMemberInGroup(
        processing_id_,
        base::BindRepeating(
            [](OutputController* controller, StreamMonitor* monitor) {
              monitor->OnStreamActive(controller);
            },
            this));
  }
}

void OutputController::Play() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.PlayTime");
  TRACE_EVENT0("audio", "OutputController::Play");
  handler_->OnLog("OutputController::Play");

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

void OutputController::StopStream() {
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

void OutputController::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.PauseTime");
  TRACE_EVENT0("audio", "OutputController::Pause");
  handler_->OnLog("OutputController::Pause");

  StopStream();

  if (state_ != kPaused)
    return;

  // Let the renderer know we've stopped.  Necessary to let PPAPI clients know
  // audio has been shutdown.  TODO(dalecurtis): This stinks.  PPAPI should have
  // a better way to know when it should exit PPB_Audio_Shared::Run().
  sync_reader_->RequestMoreData(base::TimeDelta::Max(), base::TimeTicks(), 0);

  handler_->OnControllerPaused();
}

void OutputController::Flush() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::Flush");
  handler_->OnLog("OutputController::Flush");

  if (state_ == kPlaying) {
    handler_->OnControllerError();
    return;
  }

  if (stream_) {
    stream_->Flush();
  }
}

void OutputController::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.CloseTime");
  TRACE_EVENT0("audio", "OutputController::Close");
  handler_->OnLog("OutputController::Close");

  if (state_ != kClosed) {
    StopCloseAndClearStream();
    sync_reader_->Close();

    state_ = kClosed;
  }
}

void OutputController::SetVolume(double volume) {
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
      break;
  }
}

void OutputController::ReportError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::ReportError");
  DLOG(ERROR) << "OutputController::ReportError";
  if (state_ != kClosed) {
    if (stats_tracker_)
      stats_tracker_->RegisterError();
    handler_->OnControllerError();
  }
}

int OutputController::OnMoreData(base::TimeDelta delay,
                                 base::TimeTicks delay_timestamp,
                                 int prior_frames_skipped,
                                 media::AudioBus* dest) {
  TRACE_EVENT_BEGIN1("audio", "OutputController::OnMoreData", "frames skipped",
                     prior_frames_skipped);

  stats_tracker_->OnMoreDataCalled();

  sync_reader_->Read(dest);

  const base::TimeTicks reference_time = delay_timestamp + delay;

  if (!dest->is_bitstream_format()) {
    base::AutoLock lock(snooper_lock_);
    if (!snoopers_.empty()) {
      TRACE_EVENT1("audio", "OutputController::BroadcastDataToSnoopers",
                   "reference_time (ms)",
                   (reference_time - base::TimeTicks()).InMillisecondsF());
      for (Snooper* snooper : snoopers_) {
        snooper->OnData(*dest, reference_time, volume_);
      }
    }
  }

  const int frames =
      dest->is_bitstream_format() ? dest->GetBitstreamFrames() : dest->frames();
  delay +=
      media::AudioTimestampHelper::FramesToTime(frames, params_.sample_rate());

  sync_reader_->RequestMoreData(delay, delay_timestamp, prior_frames_skipped);

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

  TRACE_EVENT_END2("audio", "OutputController::OnMoreData", "timestamp (ms)",
                   (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
                   "delay (ms)", delay.InMillisecondsF());
  return frames;
}

void OutputController::LogAudioPowerLevel(const char* call_name) {
  std::pair<float, bool> power_and_clip =
      power_monitor_.ReadCurrentPowerAndClip();
  handler_->OnLog(
      base::StringPrintf("OutputController::%s: average audio level=%.2f dBFS",
                         call_name, power_and_clip.first));
}

void OutputController::OnError() {
  // Handle error on the audio controller thread.  We defer errors for one
  // second in case they are the result of a device change; delay chosen to
  // exceed duration of device changes which take a few hundred milliseconds.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OutputController::ReportError, weak_this_for_stream_),
      base::TimeDelta::FromSeconds(1));
}

void OutputController::StopCloseAndClearStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream_ to close.
  if (stream_) {
    // Ensure any pending tasks, specific to the stream_, are canceled.
    weak_factory_for_stream_.InvalidateWeakPtrs();

    // De-register from state change callbacks if stream_ was created via
    // AudioManager.
    audio_manager_->RemoveOutputDeviceChangeListener(this);

    // Only notify and remove ourselves if startup was successful.
    if (processing_id_ && state_ != kEmpty) {
      // Don't send out activation messages for now.
      stream_monitor_coordinator_->RemoveObserver(processing_id_, this);
      // Ensure everyone monitoring us knows we're no-longer active.
      stream_monitor_coordinator_->ForEachMemberInGroup(
          processing_id_,
          base::BindRepeating(
              [](OutputController* controller, StreamMonitor* monitor) {
                monitor->OnStreamInactive(controller);
              },
              this));
    }

    StopStream();
    stream_->Close();
    stats_tracker_.reset();

    stream_ = NULL;
  }

  state_ = kEmpty;
}

const media::AudioParameters& OutputController::GetAudioParameters() const {
  return params_;
}

std::string OutputController::GetDeviceId() const {
  return output_device_id_.empty()
             ? media::AudioDeviceDescription::kDefaultDeviceId
             : output_device_id_;
}

void OutputController::StartSnooping(Snooper* snooper) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(snooper);

  // The list will only update on this thread, and only be read on the realtime
  // audio thread.
  DCHECK(!base::Contains(snoopers_, snooper));
  base::AutoLock lock(snooper_lock_);
  snoopers_.push_back(snooper);
}

void OutputController::StopSnooping(Snooper* snooper) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // The list will only update on this thread, and only be read on the realtime
  // audio thread.
  const auto it = std::find(snoopers_.begin(), snoopers_.end(), snooper);
  DCHECK(it != snoopers_.end());
  // We also don't care about ordering, so swap and pop rather than erase.
  base::AutoLock lock(snooper_lock_);
  *it = snoopers_.back();
  snoopers_.pop_back();
}

void OutputController::StartMuting() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!disable_local_output_)
    ToggleLocalOutput();
}

void OutputController::StopMuting() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (disable_local_output_)
    ToggleLocalOutput();
}

void OutputController::ToggleLocalOutput() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  disable_local_output_ = !disable_local_output_;

  // If there is an active |stream_|, close it and re-create either: 1) a fake
  // stream to prevent local audio output, or 2) a normal AudioOutputStream.
  if (stream_) {
    const bool restore_playback = (state_ == kPlaying);
    RecreateStream(RecreateReason::LOCAL_OUTPUT_TOGGLE);
    if (state_ == kCreated && restore_playback)
      Play();
  }
}

void OutputController::OnMemberJoinedGroup(StreamMonitor* monitor) {
  // We're only observing the group when we're active.
  monitor->OnStreamActive(this);
}

void OutputController::OnMemberLeftGroup(StreamMonitor* monitor) {
  // Do nothing. The monitor will have already cleaned up.
}

void OutputController::OnDeviceChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::OnDeviceChange");

  if (disable_local_output_)
    return;  // No actions need to be taken while local output is disabled.

  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.DeviceChangeTime");

  auto state_to_string = [](State state) {
    switch (state) {
      case kEmpty:
        return "empty";
      case kCreated:
        return "created";
      case kPlaying:
        return "playing";
      case kPaused:
        return "paused";
      case kClosed:
        return "closed";
      case kError:
        return "error";
    }
    return "unknown";
  };

  handler_->OnLog(
      base::StringPrintf("OutputController::OnDeviceChange while in state: %s",
                         state_to_string(state_)));

  // TODO(dalecurtis): Notify the renderer side that a device change has
  // occurred.  Currently querying the hardware information here will lead to
  // crashes on OSX.  See http://crbug.com/158170.

  const bool restore_playback = (state_ == kPlaying);
  // TODO(crbug.com/896484): This will also add a UMA timing measurement to
  // "Media.AudioOutputController.ChangeTime" which maybe is not desired?
  RecreateStreamWithTimingUMA(RecreateReason::DEVICE_CHANGE);
  if (state_ == kCreated && restore_playback)
    Play();
}

std::pair<float, bool> OutputController::ReadCurrentPowerAndClip() {
  DCHECK(will_monitor_audio_levels());
  return power_monitor_.ReadCurrentPowerAndClip();
}

}  // namespace audio
