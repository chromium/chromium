// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_controller.h"

#include <inttypes.h>
#include <stdio.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/media_buildflags.h"
#include "services/audio/device_listener_output_stream.h"
#include "services/audio/stream_monitor.h"

namespace audio {

namespace {

// Time in seconds between two successive measurements of audio power levels.
constexpr base::TimeDelta kPowerMonitorLogInterval = base::Seconds(15);

const char* StateToString(OutputController::State state) {
  switch (state) {
    case OutputController::kEmpty:
      return "empty";
    case OutputController::kCreated:
      return "created";
    case OutputController::kPlaying:
      return "playing";
    case OutputController::kPaused:
      return "paused";
    case OutputController::kClosed:
      return "closed";
    case OutputController::kError:
      return "error";
  }
  return "unknown";
}

const char* ErrorTypeToString(
    media::AudioOutputStream::AudioSourceCallback::ErrorType type) {
  switch (type) {
    case media::AudioOutputStream::AudioSourceCallback::ErrorType::kUnknown:
      return "Unknown";
    case media::AudioOutputStream::AudioSourceCallback::ErrorType::
        kDeviceChange:
      return "DeviceChange";
  }
  return "Invalid";
}

}  // namespace

OutputController::ErrorStatisticsTracker::ErrorStatisticsTracker(
    OutputController* controller)
    : controller_(controller),
      start_time_(base::TimeTicks::Now()),
      on_more_io_data_called_(0) {
  // WedgeCheck() will look to see if |on_more_io_data_called_| is true after
  // the timeout expires and log this as a UMA stat. If the stream is
  // paused/closed before the timer fires, nothing is logged.
  wedge_timer_.Start(FROM_HERE, base::Seconds(5), this,
                     &ErrorStatisticsTracker::WedgeCheck);
}

OutputController::ErrorStatisticsTracker::~ErrorStatisticsTracker() {
  const base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_LONG_TIMES("Media.OutputStreamDuration", duration);
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputController.CallbackError",
                        error_during_callback_);
  if (controller_) {
    controller_->SendLogMessage("StopStream => (duration=%" PRId64 " sec)",
                                duration.InSeconds());
    controller_->SendLogMessage("StopStream => (error_during_callback=%s)",
                                error_during_callback_ ? "true" : "false");
  }
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
  if (on_more_io_data_called_.IsOne()) {
    if (controller_)
      controller_->SendLogMessage("WedgeCheck => (stream is alive)");
  }
}

OutputController::OutputController(
    media::AudioManager* audio_manager,
    EventHandler* handler,
    const media::AudioParameters& params,
    const std::string& output_device_id,
    SyncReader* sync_reader,
    ManagedDeviceOutputStreamCreateCallback
        managed_device_output_stream_create_callback)
    : audio_manager_(audio_manager),
      params_(params),
      managed_device_output_stream_create_callback_(
          std::move(managed_device_output_stream_create_callback)),
      handler_(handler),
      task_runner_(audio_manager->GetTaskRunner()),
      construction_time_(base::TimeTicks::Now()),
      output_device_id_(output_device_id),
      stream_(nullptr),
      disable_local_output_(false),
      volume_(1.0),
      state_(kEmpty),
      sync_reader_(sync_reader),
      power_monitor_(params.sample_rate(),
                     base::Milliseconds(kPowerMeasurementTimeConstantMillis)) {
  DCHECK(audio_manager);
  DCHECK(handler_);
  DCHECK(sync_reader_);
  DCHECK(task_runner_.get());
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
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));
  RecreateStream(RecreateReason::INITIAL_STREAM);
  SendLogMessage("%s => (state=%s)", __func__, StateToString(state_));
  return state_ == kCreated;
}

// static
void OutputController::ReportStreamCreationUma(
    OutputController::RecreateReason reason,
    StreamCreationResult result) {
  switch (reason) {
    case RecreateReason::INITIAL_STREAM:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.AudioOutputController."
          "ProxyStreamCreationResultForDeviceChange",
          result);
      return;
    case RecreateReason::DEVICE_CHANGE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.AudioOutputController.ProxyStreamCreationResult", result);
      return;
    case RecreateReason::LOCAL_OUTPUT_TOGGLE:
      return;  // Not counted in UMAs.
  }
  return;
}

// static
const char* OutputController::RecreateReasonToString(
    OutputController::RecreateReason reason) {
  switch (reason) {
    case RecreateReason::INITIAL_STREAM:
      return "INITIAL_STREAM";
    case RecreateReason::DEVICE_CHANGE:
      return "DEVICE_CHANGE";
    case RecreateReason::LOCAL_OUTPUT_TOGGLE:
      return "LOCAL_OUTPUT_TOGGLE";
  }
  return "Invalid";
}

void OutputController::RecreateStream(OutputController::RecreateReason reason) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("audio", "OutputController::RecreateStream", "reason",
               RecreateReasonToString(reason));

  SendLogMessage("RecreateStream({reason = %s}, {params = [%s]}, [state = %s])",
                 RecreateReasonToString(reason),
                 params_.AsHumanReadableString().c_str(),
                 StateToString(state_));

  // Close() can be called before Create() is executed.
  if (state_ == kClosed)
    return;

  StopCloseAndClearStream();
  DCHECK_EQ(kEmpty, state_);

  if (disable_local_output_) {
    SendLogMessage("%s => (WARNING: local output disabed, using a fake stream)",
                   __func__);
    // Create a fake AudioOutputStream that will continue pumping the audio
    // data, but does not play it out anywhere. Pumping the audio data is
    // necessary because video playback is synchronized to the audio stream and
    // will freeze otherwise.
    media::AudioParameters mute_params = params_;
    mute_params.set_format(media::AudioParameters::AUDIO_FAKE);
    stream_ = audio_manager_->MakeAudioOutputStream(
        mute_params, std::string(),
        /*log_callback, not used*/ base::DoNothing());
  } else if (managed_device_output_stream_create_callback_) {
    stream_ = managed_device_output_stream_create_callback_.Run(
        output_device_id_, params_,
        base::BindRepeating(&OutputController::ProcessDeviceChange,
                            base::Unretained(this)));
  } else {
    media::AudioOutputStream* stream =
        audio_manager_->MakeAudioOutputStreamProxy(params_, output_device_id_);
    if (stream) {
      // ProcessDeviceChange must close |stream_|.
      stream_ = new DeviceListenerOutputStream(
          audio_manager_, stream,
          base::BindRepeating(&OutputController::ProcessDeviceChange,
                              base::Unretained(this)));
    }
  }

  if (!stream_) {
    SendLogMessage("%s => (ERROR: failed to create output stream)", __func__);
    state_ = kError;
    ReportStreamCreationUma(reason, StreamCreationResult::kCreateFailed);
    handler_->OnControllerError();
    return;
  }

  if (!stream_->Open()) {
    SendLogMessage("%s => (ERROR: failed to open the created output stream)",
                   __func__);
    StopCloseAndClearStream();
    state_ = kError;
    ReportStreamCreationUma(reason, StreamCreationResult::kOpenFailed);
    handler_->OnControllerError();
    return;
  }

  // Finally set the state to kCreated. Note that, it is possible that the
  // stream is fake in this state due to the fallback mechanism in the audio
  // output dispatcher which falls back to a fake stream if audio parameters
  // are invalid or if a physical stream can't be opened for some reason.
  state_ = kCreated;
  ReportStreamCreationUma(reason, StreamCreationResult::kOk);

  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);
}

void OutputController::Play() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::Play");
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;

  StartStream();
}

void OutputController::StartStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kCreated || state_ == kPaused);

  // Ask for first packet.
  sync_reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), {});

  state_ = kPlaying;
  SendLogMessage("%s => (state=%s)", __func__, StateToString(state_));

  if (will_monitor_audio_levels()) {
    last_audio_level_log_time_ = base::TimeTicks::Now();
  }

  stats_tracker_.emplace(this);

  stream_->Start(this);

  handler_->OnControllerPlaying();
}

void OutputController::StopStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kPlaying) {
    stream_->Stop();
    // Destructor of ErrorStatisticsTracker also adds a log message.
    stats_tracker_.reset();

    if (will_monitor_audio_levels()) {
      LogAudioPowerLevel(__func__);
    }

    // A stopped stream is silent, and power_montior_.Scan() is no longer being
    // called; so we must reset the power monitor.
    power_monitor_.Reset();

    state_ = kPaused;
  }
}

void OutputController::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::Pause");
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));

  StopStream();

  if (state_ != kPaused)
    return;

  // Let the renderer know we've stopped.  Necessary to let PPAPI clients know
  // audio has been shutdown.  TODO(dalecurtis): This stinks.  PPAPI should have
  // a better way to know when it should exit PPB_Audio_Shared::Run().
  sync_reader_->RequestMoreData(base::TimeDelta::Max(), base::TimeTicks(), {});

  handler_->OnControllerPaused();
  SendLogMessage("%s => (state=%s)", __func__, StateToString(state_));
}

void OutputController::Flush() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::Flush");
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));

  if (state_ == kPlaying) {
    handler_->OnControllerError();
    return;
  }

  if (stream_) {
    stream_->Flush();
  }
  SendLogMessage("%s => (state=%s)", __func__, StateToString(state_));
}

void OutputController::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::Close");
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));

  if (state_ != kClosed) {
    StopCloseAndClearStream();
    sync_reader_->Close();
    state_ = kClosed;
  }
  SendLogMessage("%s => (state=%s)", __func__, StateToString(state_));
}

void OutputController::SetVolume(double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SendLogMessage("%s({volume=%.2f} [state=%s])", __func__, volume,
                 StateToString(state_));

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

int OutputController::OnMoreData(base::TimeDelta delay,
                                 base::TimeTicks delay_timestamp,
                                 const media::AudioGlitchInfo& glitch_info,
                                 media::AudioBus* dest) {
  return OnMoreData(delay, delay_timestamp, glitch_info, dest, false);
}

int OutputController::OnMoreData(base::TimeDelta delay,
                                 base::TimeTicks delay_timestamp,
                                 const media::AudioGlitchInfo& glitch_info,
                                 media::AudioBus* dest,
                                 bool is_mixing) {
  TRACE_EVENT("audio", "OutputController::OnMoreData", "this",
              static_cast<void*>(this), "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
              "playout_delay (ms)", delay.InMillisecondsF());
  glitch_info.MaybeAddTraceEvent();

  stats_tracker_->OnMoreDataCalled();

  const bool received_data = sync_reader_->Read(dest, is_mixing);

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

  sync_reader_->RequestMoreData(delay, delay_timestamp, glitch_info);

#if !BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  constexpr bool is_bitstream = false;
#else
  const bool is_bitstream = params_.IsBitstreamFormat();
#endif

  // Skip scanning `dest` when it's zero'ed to due to timeout glitches. This
  // gives more accurate results from `power_monitor_`.
  if (will_monitor_audio_levels() && received_data && !is_bitstream) {
    // Note: this code path should never be hit when using bitstream streams.
    // Scan doesn't expect compressed audio, so it may go out of bounds trying
    // to read |frames| frames of PCM data.
    CHECK(!params_.IsBitstreamFormat());
    power_monitor_.Scan(*dest, frames);

    const auto now = base::TimeTicks::Now();
    if ((now - last_audio_level_log_time_) > kPowerMonitorLogInterval) {
      LogAudioPowerLevel(__func__);
      last_audio_level_log_time_ = now;
    }
  }

  return frames;
}

void OutputController::SendLogMessage(const char* format, ...) {
  if (!handler_)
    return;
  va_list args;
  va_start(args, format);
  handler_->OnLog("AOC::" + base::StringPrintV(format, args) +
                  base::StringPrintf(" [this=0x%" PRIXPTR "]",
                                     reinterpret_cast<uintptr_t>(this)));
  va_end(args);
}

void OutputController::LogAudioPowerLevel(const char* call_name) {
  std::pair<float, bool> power_and_clip =
      power_monitor_.ReadCurrentPowerAndClip();
  SendLogMessage("%s => (average audio level=%.2f dBFS)", call_name,
                 power_and_clip.first);
}

void OutputController::OnError(ErrorType type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SendLogMessage("%s({type=%s} [state=%s])", __func__, ErrorTypeToString(type),
                 StateToString(state_));
  TRACE_EVENT0("audio", "OutputController::OnError");
  DLOG(ERROR) << "OutputController::OnError";
  if (state_ != kClosed) {
    if (stats_tracker_)
      stats_tracker_->RegisterError();
    handler_->OnControllerError();
  }
}

void OutputController::StopCloseAndClearStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream_ to close.
  if (stream_) {
    StopStream();
    stream_->Close();
    stream_ = nullptr;
  }

  state_ = kEmpty;
}

const media::AudioParameters& OutputController::GetAudioParameters() const {
  return params_;
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
  const auto it = base::ranges::find(snoopers_, snooper);
  CHECK(it != snoopers_.end(), base::NotFatalUntil::M130);
  // We also don't care about ordering, so swap and pop rather than erase.
  base::AutoLock lock(snooper_lock_);
  *it = snoopers_.back();
  snoopers_.pop_back();
}

void OutputController::StartMuting() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));

  if (!disable_local_output_) {
    ToggleLocalOutput();
  }
}

void OutputController::StopMuting() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));

  if (disable_local_output_) {
    ToggleLocalOutput();
  }
}

void OutputController::ToggleLocalOutput() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  disable_local_output_ = !disable_local_output_;

  SendLogMessage("%s({disable_local_output=%s} [state=%s])", __func__,
                 disable_local_output_ ? "true" : "false",
                 StateToString(state_));

  // If there is an active |stream_|, close it and re-create either: 1) a fake
  // stream to prevent local audio output, or 2) a normal AudioOutputStream.
  if (stream_) {
    const bool restore_playback = (state_ == kPlaying);
    RecreateStream(RecreateReason::LOCAL_OUTPUT_TOGGLE);
    if (state_ == kCreated && restore_playback)
      StartStream();
  }
}

void OutputController::ProcessDeviceChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SendLogMessage("%s([state=%s])", __func__, StateToString(state_));
  TRACE_EVENT0("audio", "OutputController::ProcessDeviceChange");

  DCHECK(!disable_local_output_);

  // TODO(dalecurtis): Notify the renderer side that a device change has
  // occurred.  Currently querying the hardware information here will lead to
  // crashes on OSX.  See http://crbug.com/158170.

  const bool restore_playback = (state_ == kPlaying);
  RecreateStream(RecreateReason::DEVICE_CHANGE);
  if (state_ == kCreated && restore_playback)
    StartStream();
}

std::pair<float, bool> OutputController::ReadCurrentPowerAndClip() {
  DCHECK(will_monitor_audio_levels());
  return power_monitor_.ReadCurrentPowerAndClip();
}

void OutputController::SwitchAudioOutputDeviceId(
    const std::string& new_output_device_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (output_device_id_ == new_output_device_id) {
    return;
  }

  output_device_id_ = new_output_device_id;
  ProcessDeviceChange();
}

}  // namespace audio
