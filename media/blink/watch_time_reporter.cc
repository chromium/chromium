// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/watch_time_reporter.h"

#include <numeric>

#include "base/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/watch_time_keys.h"

namespace media {

// The minimum width and height of videos to report watch time metrics for.
constexpr gfx::Size kMinimumVideoSize = gfx::Size(200, 140);

static bool IsOnBatteryPower() {
  if (base::PowerMonitor::IsInitialized())
    return base::PowerMonitor::IsOnBatteryPower();
  return false;
}

// Helper function for managing property changes. If the watch time timer is
// running it sets the pending value otherwise it sets the current value and
// then returns true if the component needs finalize.
enum class PropertyAction { kNoActionRequired, kFinalizeRequired };
template <typename T>
PropertyAction HandlePropertyChange(T new_value,
                                    bool is_timer_running,
                                    WatchTimeComponent<T>* component) {
  if (!component)
    return PropertyAction::kNoActionRequired;

  if (is_timer_running)
    component->SetPendingValue(new_value);
  else
    component->SetCurrentValue(new_value);

  return component->NeedsFinalize() ? PropertyAction::kFinalizeRequired
                                    : PropertyAction::kNoActionRequired;
}

WatchTimeReporter::WatchTimeReporter(
    mojom::PlaybackPropertiesPtr properties,
    const gfx::Size& natural_size,
    GetMediaTimeCB get_media_time_cb,
    GetPipelineStatsCB get_pipeline_stats_cb,
    mojom::MediaMetricsProvider* provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock)
    : WatchTimeReporter(std::move(properties),
                        false /* is_background */,
                        false /* is_muted */,
                        natural_size,
                        std::move(get_media_time_cb),
                        std::move(get_pipeline_stats_cb),
                        provider,
                        task_runner,
                        tick_clock) {}

WatchTimeReporter::WatchTimeReporter(
    mojom::PlaybackPropertiesPtr properties,
    bool is_background,
    bool is_muted,
    const gfx::Size& natural_size,
    GetMediaTimeCB get_media_time_cb,
    GetPipelineStatsCB get_pipeline_stats_cb,
    mojom::MediaMetricsProvider* provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TickClock* tick_clock)
    : properties_(std::move(properties)),
      is_background_(is_background),
      is_muted_(is_muted),
      get_media_time_cb_(std::move(get_media_time_cb)),
      get_pipeline_stats_cb_(std::move(get_pipeline_stats_cb)),
      reporting_timer_(tick_clock),
      natural_size_(natural_size) {
  DCHECK(get_media_time_cb_);
  DCHECK(get_pipeline_stats_cb_);
  DCHECK(properties_->has_audio || properties_->has_video);
  DCHECK_EQ(is_background, properties_->is_background);

  // The background reporter receives play/pause events instead of visibility
  // changes, so it must always be visible to function correctly.
  if (is_background_)
    DCHECK(is_visible_);

  // The muted reporter receives play/pause events instead of volume changes, so
  // its volume must always be audible to function correctly.
  if (is_muted_)
    DCHECK_EQ(volume_, 1.0);

  base::PowerMonitor::AddObserver(this);

  provider->AcquireWatchTimeRecorder(properties_->Clone(),
                                     recorder_.BindNewPipeAndPassReceiver());

  reporting_timer_.SetTaskRunner(task_runner);

  base_component_ = CreateBaseComponent();
  power_component_ = CreatePowerComponent();
  if (!is_background_) {
    controls_component_ = CreateControlsComponent();
    if (properties_->has_video)
      display_type_component_ = CreateDisplayTypeComponent();
  }

  // If this is a sub-reporter we're done.
  if (is_background_ || is_muted_)
    return;

  // Background watch time is reported by creating an background only watch time
  // reporter which receives play when hidden and pause when shown. This avoids
  // unnecessary complexity inside the UpdateWatchTime() for handling this case.
  auto prop_copy = properties_.Clone();
  prop_copy->is_background = true;
  background_reporter_.reset(new WatchTimeReporter(
      std::move(prop_copy), true /* is_background */, false /* is_muted */,
      natural_size_, get_media_time_cb_, get_pipeline_stats_cb_, provider,
      task_runner, tick_clock));

  // Muted watch time is only reported for audio+video playback.
  if (!properties_->has_video || !properties_->has_audio)
    return;

  // Similar to the above, muted watch time is reported by creating a muted only
  // watch time reporter which receives play when muted and pause when audible.
  prop_copy = properties_.Clone();
  prop_copy->is_muted = true;
  muted_reporter_.reset(new WatchTimeReporter(
      std::move(prop_copy), false /* is_background */, true /* is_muted */,
      natural_size_, get_media_time_cb_, get_pipeline_stats_cb_, provider,
      task_runner, tick_clock));
}

WatchTimeReporter::~WatchTimeReporter() {
  background_reporter_.reset();
  muted_reporter_.reset();

  // This is our last chance, so finalize now if there's anything remaining.
  in_shutdown_ = true;
  MaybeFinalizeWatchTime(FinalizeTime::IMMEDIATELY);
  base::PowerMonitor::RemoveObserver(this);
}

void WatchTimeReporter::OnPlaying() {
  if (background_reporter_ && !is_visible_)
    background_reporter_->OnPlaying();
  if (muted_reporter_ && !volume_)
    muted_reporter_->OnPlaying();

  is_playing_ = true;
  is_seeking_ = false;
  MaybeStartReportingTimer(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnPaused() {
  if (background_reporter_)
    background_reporter_->OnPaused();
  if (muted_reporter_)
    muted_reporter_->OnPaused();

  is_playing_ = false;
  MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnSeeking() {
  if (background_reporter_)
    background_reporter_->OnSeeking();
  if (muted_reporter_)
    muted_reporter_->OnSeeking();

  // Seek is a special case that does not have hysteresis, when this is called
  // the seek is imminent, so finalize the previous playback immediately.
  is_seeking_ = true;
  MaybeFinalizeWatchTime(FinalizeTime::IMMEDIATELY);
}

void WatchTimeReporter::OnVolumeChange(double volume) {
  if (background_reporter_)
    background_reporter_->OnVolumeChange(volume);

  // The muted reporter should never receive volume changes.
  DCHECK(!is_muted_);

  const double old_volume = volume_;
  volume_ = volume;

  // We're only interesting in transitions in and out of the muted state.
  if (!old_volume && volume) {
    if (muted_reporter_)
      muted_reporter_->OnPaused();
    MaybeStartReportingTimer(get_media_time_cb_.Run());
  } else if (old_volume && !volume_) {
    if (muted_reporter_ && is_playing_)
      muted_reporter_->OnPlaying();
    MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
  }
}

void WatchTimeReporter::OnShown() {
  // The background reporter should never receive visibility changes.
  DCHECK(!is_background_);

  if (background_reporter_)
    background_reporter_->OnPaused();
  if (muted_reporter_)
    muted_reporter_->OnShown();

  is_visible_ = true;
  MaybeStartReportingTimer(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnHidden() {
  // The background reporter should never receive visibility changes.
  DCHECK(!is_background_);

  if (background_reporter_ && is_playing_)
    background_reporter_->OnPlaying();
  if (muted_reporter_)
    muted_reporter_->OnHidden();

  is_visible_ = false;
  MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnError(PipelineStatus status) {
  // Since playback should have stopped by this point, go ahead and send the
  // error directly instead of on the next timer tick. It won't be recorded
  // until finalization anyways.
  recorder_->OnError(status);
  if (background_reporter_)
    background_reporter_->OnError(status);
  if (muted_reporter_)
    muted_reporter_->OnError(status);
}

void WatchTimeReporter::OnUnderflow() {
  if (background_reporter_)
    background_reporter_->OnUnderflow();
  if (muted_reporter_)
    muted_reporter_->OnUnderflow();

  if (!reporting_timer_.IsRunning())
    return;

  if (!pending_underflow_events_.empty())
    DCHECK_NE(pending_underflow_events_.back().duration, kNoTimestamp);

  // In the event of a pending finalize, we don't want to count underflow events
  // that occurred after the finalize time. Yet if the finalize is canceled we
  // want to ensure they are all recorded.
  pending_underflow_events_.push_back(
      {false, get_media_time_cb_.Run(), kNoTimestamp});
}

void WatchTimeReporter::OnUnderflowComplete(base::TimeDelta elapsed) {
  if (background_reporter_)
    background_reporter_->OnUnderflowComplete(elapsed);
  if (muted_reporter_)
    muted_reporter_->OnUnderflowComplete(elapsed);

  if (!reporting_timer_.IsRunning())
    return;

  // Drop this underflow completion if we don't have a corresponding underflow
  // start event; this can happen if a finalize occurs between the underflow and
  // the completion.
  if (pending_underflow_events_.empty())
    return;

  // There should only ever be one outstanding underflow, so stick the duration
  // in the last underflow event.
  DCHECK_EQ(pending_underflow_events_.back().duration, kNoTimestamp);
  pending_underflow_events_.back().duration = elapsed;
}

void WatchTimeReporter::OnNativeControlsEnabled() {
  OnNativeControlsChanged(true);
}

void WatchTimeReporter::OnNativeControlsDisabled() {
  OnNativeControlsChanged(false);
}

void WatchTimeReporter::OnDisplayTypeInline() {
  OnDisplayTypeChanged(DisplayType::kInline);
}

void WatchTimeReporter::OnDisplayTypeFullscreen() {
  OnDisplayTypeChanged(DisplayType::kFullscreen);
}

void WatchTimeReporter::OnDisplayTypePictureInPicture() {
  OnDisplayTypeChanged(DisplayType::kPictureInPicture);
}

void WatchTimeReporter::UpdateSecondaryProperties(
    mojom::SecondaryPlaybackPropertiesPtr secondary_properties) {
  // Flush any unrecorded watch time before updating the secondary properties to
  // ensure the UKM record is finalized with up-to-date watch time information.
  if (reporting_timer_.IsRunning())
    RecordWatchTime();

  recorder_->UpdateSecondaryProperties(secondary_properties.Clone());
  if (background_reporter_) {
    background_reporter_->UpdateSecondaryProperties(
        secondary_properties.Clone());
  }
  if (muted_reporter_)
    muted_reporter_->UpdateSecondaryProperties(secondary_properties.Clone());

  // A change in resolution may affect ShouldReportingTimerRun().
  bool original_should_run = ShouldReportingTimerRun();
  natural_size_ = secondary_properties->natural_size;
  bool should_run = ShouldReportingTimerRun();
  if (original_should_run != should_run) {
    if (should_run) {
      MaybeStartReportingTimer(get_media_time_cb_.Run());
    } else {
      MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
    }
  }
}

void WatchTimeReporter::SetAutoplayInitiated(bool autoplay_initiated) {
  recorder_->SetAutoplayInitiated(autoplay_initiated);
  if (background_reporter_)
    background_reporter_->SetAutoplayInitiated(autoplay_initiated);
  if (muted_reporter_)
    muted_reporter_->SetAutoplayInitiated(autoplay_initiated);
}

void WatchTimeReporter::OnDurationChanged(base::TimeDelta duration) {
  recorder_->OnDurationChanged(duration);
  if (background_reporter_)
    background_reporter_->OnDurationChanged(duration);
  if (muted_reporter_)
    muted_reporter_->OnDurationChanged(duration);
}

void WatchTimeReporter::OnPowerStateChange(bool on_battery_power) {
  if (HandlePropertyChange<bool>(on_battery_power, reporting_timer_.IsRunning(),
                                 power_component_.get()) ==
      PropertyAction::kFinalizeRequired) {
    RestartTimerForHysteresis();
  }
}

void WatchTimeReporter::OnNativeControlsChanged(bool has_native_controls) {
  if (muted_reporter_)
    muted_reporter_->OnNativeControlsChanged(has_native_controls);

  if (HandlePropertyChange<bool>(
          has_native_controls, reporting_timer_.IsRunning(),
          controls_component_.get()) == PropertyAction::kFinalizeRequired) {
    RestartTimerForHysteresis();
  }
}

void WatchTimeReporter::OnDisplayTypeChanged(DisplayType display_type) {
  if (muted_reporter_)
    muted_reporter_->OnDisplayTypeChanged(display_type);

  if (HandlePropertyChange<DisplayType>(
          display_type, reporting_timer_.IsRunning(),
          display_type_component_.get()) == PropertyAction::kFinalizeRequired) {
    RestartTimerForHysteresis();
  }
}

bool WatchTimeReporter::ShouldReportWatchTime() const {
  // Report listen time or watch time for videos of sufficient size.
  return properties_->has_video
             ? (natural_size_.height() >= kMinimumVideoSize.height() &&
                natural_size_.width() >= kMinimumVideoSize.width())
             : properties_->has_audio;
}

bool WatchTimeReporter::ShouldReportingTimerRun() const {
  // TODO(dalecurtis): We should only consider |volume_| when there is actually
  // an audio track; requires updating lots of tests to fix.
  return ShouldReportWatchTime() && is_playing_ && volume_ && is_visible_ &&
         !in_shutdown_ && !is_seeking_ && has_valid_start_timestamp_;
}

void WatchTimeReporter::MaybeStartReportingTimer(
    base::TimeDelta start_timestamp) {
  DCHECK_GE(start_timestamp, base::TimeDelta());

  // It's possible for |current_time| to be kInfiniteDuration here if the page
  // seeks to kInfiniteDuration (2**64 - 1) when Duration() is infinite. There
  // is no possible elapsed watch time when this occurs, so don't start the
  // WatchTimeReporter at this time. If a later seek puts us earlier in the
  // stream this method will be called again after OnSeeking().
  has_valid_start_timestamp_ = start_timestamp != kInfiniteDuration;

  // Don't start the timer if our state indicates we shouldn't; this check is
  // important since the various event handlers do not have to care about the
  // state of other events.
  const bool should_start = ShouldReportingTimerRun();
  if (reporting_timer_.IsRunning()) {
    base_component_->SetPendingValue(should_start);
    return;
  }

  base_component_->SetCurrentValue(should_start);
  if (!should_start)
    return;

  if (properties_->has_video) {
    initial_stats_ = get_pipeline_stats_cb_.Run();
    last_stats_ = PipelineStatistics();
  }

  ResetUnderflowState();
  base_component_->OnReportingStarted(start_timestamp);
  power_component_->OnReportingStarted(start_timestamp);

  if (controls_component_)
    controls_component_->OnReportingStarted(start_timestamp);
  if (display_type_component_)
    display_type_component_->OnReportingStarted(start_timestamp);

  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::MaybeFinalizeWatchTime(FinalizeTime finalize_time) {
  if (HandlePropertyChange<bool>(
          ShouldReportingTimerRun(), reporting_timer_.IsRunning(),
          base_component_.get()) == PropertyAction::kNoActionRequired) {
    return;
  }

  if (finalize_time == FinalizeTime::IMMEDIATELY) {
    UpdateWatchTime();
    return;
  }

  // Always restart the timer when finalizing, so that we allow for the full
  // length of |kReportingInterval| to elapse for hysteresis purposes.
  DCHECK_EQ(finalize_time, FinalizeTime::ON_NEXT_UPDATE);
  RestartTimerForHysteresis();
}

void WatchTimeReporter::RestartTimerForHysteresis() {
  // Restart the reporting timer so the full hysteresis is afforded.
  DCHECK(reporting_timer_.IsRunning());
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::RecordWatchTime() {
  // If we're finalizing, use the media time at time of finalization.
  const base::TimeDelta current_timestamp =
      base_component_->NeedsFinalize() ? base_component_->end_timestamp()
                                       : get_media_time_cb_.Run();

  // Pass along any underflow events which have occurred since the last report.
  if (!pending_underflow_events_.empty()) {
    const int last_underflow_count = total_underflow_count_;
    const int last_completed_underflow_count = total_completed_underflow_count_;

    for (auto& ufe : pending_underflow_events_) {
      // Since the underflow occurred after finalize, ignore the event and mark
      // it for deletion.
      if (ufe.timestamp > current_timestamp) {
        ufe.reported = true;
        ufe.duration = base::TimeDelta();
        continue;
      }

      if (!ufe.reported) {
        ufe.reported = true;
        ++total_underflow_count_;
      }

      // Drop any rebuffer completions that took more than a minute. For our
      // purposes these are considered as timeouts. We want a maximum since
      // rebuffer duration is in real time and not media time, which means if
      // the rebuffer spans a suspend/resume the time can be arbitrarily long.
      constexpr base::TimeDelta kMaximumRebufferDuration =
          base::TimeDelta::FromMinutes(1);
      if (ufe.duration != kNoTimestamp &&
          ufe.duration <= kMaximumRebufferDuration) {
        ++total_completed_underflow_count_;
        total_underflow_duration_ += ufe.duration;
      }
    }

    base::EraseIf(pending_underflow_events_, [](const UnderflowEvent& ufe) {
      return ufe.reported && ufe.duration != kNoTimestamp;
    });

    if (last_underflow_count != total_underflow_count_)
      recorder_->UpdateUnderflowCount(total_underflow_count_);
    if (last_completed_underflow_count != total_completed_underflow_count_) {
      recorder_->UpdateUnderflowDuration(total_completed_underflow_count_,
                                         total_underflow_duration_);
    }
  }

  if (properties_->has_video) {
    auto stats = get_pipeline_stats_cb_.Run();
    DCHECK_GE(stats.video_frames_decoded, initial_stats_.video_frames_decoded);
    DCHECK_GE(stats.video_frames_dropped, initial_stats_.video_frames_dropped);

    // Offset the stats based on where they were when we started reporting.
    stats.video_frames_decoded -= initial_stats_.video_frames_decoded;
    stats.video_frames_dropped -= initial_stats_.video_frames_dropped;

    // Only send updates.
    if (last_stats_.video_frames_decoded != stats.video_frames_decoded ||
        last_stats_.video_frames_dropped != stats.video_frames_dropped) {
      recorder_->UpdateVideoDecodeStats(stats.video_frames_decoded,
                                        stats.video_frames_dropped);
      last_stats_ = stats;
    }
  }

  // Record watch time for all components.
  base_component_->RecordWatchTime(current_timestamp);
  power_component_->RecordWatchTime(current_timestamp);
  if (display_type_component_)
    display_type_component_->RecordWatchTime(current_timestamp);
  if (controls_component_)
    controls_component_->RecordWatchTime(current_timestamp);

  // Update the last timestamp with the current timestamp.
  recorder_->OnCurrentTimestampChanged(current_timestamp);
}

void WatchTimeReporter::UpdateWatchTime() {
  // First record watch time.
  RecordWatchTime();

  // Second, process any pending finalize events.
  std::vector<WatchTimeKey> keys_to_finalize;
  if (power_component_->NeedsFinalize())
    power_component_->Finalize(&keys_to_finalize);
  if (display_type_component_ && display_type_component_->NeedsFinalize())
    display_type_component_->Finalize(&keys_to_finalize);
  if (controls_component_ && controls_component_->NeedsFinalize())
    controls_component_->Finalize(&keys_to_finalize);

  // Then finalize the base component.
  if (!base_component_->NeedsFinalize()) {
    if (!keys_to_finalize.empty())
      recorder_->FinalizeWatchTime(keys_to_finalize);
    return;
  }

  // Always send finalize, even if we don't currently have any data, it's
  // harmless to send since nothing will be logged if we've already finalized.
  base_component_->Finalize(&keys_to_finalize);
  recorder_->FinalizeWatchTime({});

  // Stop the timer if this is supposed to be our last tick.
  ResetUnderflowState();
  reporting_timer_.Stop();
}

void WatchTimeReporter::ResetUnderflowState() {
  total_underflow_count_ = total_completed_underflow_count_ = 0;
  total_underflow_duration_ = base::TimeDelta();
  pending_underflow_events_.clear();
}

#define NORMAL_KEY(key)                                                     \
  ((properties_->has_video && properties_->has_audio)                       \
       ? (is_background_ ? WatchTimeKey::kAudioVideoBackground##key         \
                         : (is_muted_ ? WatchTimeKey::kAudioVideoMuted##key \
                                      : WatchTimeKey::kAudioVideo##key))    \
       : properties_->has_video                                             \
             ? (is_background_ ? WatchTimeKey::kVideoBackground##key        \
                               : WatchTimeKey::kVideo##key)                 \
             : (is_background_ ? WatchTimeKey::kAudioBackground##key        \
                               : WatchTimeKey::kAudio##key))

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreateBaseComponent() {
  std::vector<WatchTimeKey> keys_to_finalize;
  keys_to_finalize.emplace_back(NORMAL_KEY(All));
  if (properties_->is_mse)
    keys_to_finalize.emplace_back(NORMAL_KEY(Mse));
  else
    keys_to_finalize.emplace_back(NORMAL_KEY(Src));

  if (properties_->is_eme)
    keys_to_finalize.emplace_back(NORMAL_KEY(Eme));

  if (properties_->is_embedded_media_experience)
    keys_to_finalize.emplace_back(NORMAL_KEY(EmbeddedExperience));

  return std::make_unique<WatchTimeComponent<bool>>(
      false, std::move(keys_to_finalize),
      WatchTimeComponent<bool>::ValueToKeyCB(), get_media_time_cb_,
      recorder_.get());
}

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreatePowerComponent() {
  std::vector<WatchTimeKey> keys_to_finalize{NORMAL_KEY(Battery),
                                             NORMAL_KEY(Ac)};

  return std::make_unique<WatchTimeComponent<bool>>(
      IsOnBatteryPower(), std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetPowerKey,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

WatchTimeKey WatchTimeReporter::GetPowerKey(bool is_on_battery_power) {
  return is_on_battery_power ? NORMAL_KEY(Battery) : NORMAL_KEY(Ac);
}
#undef NORMAL_KEY

#define FOREGROUND_KEY(key)                                 \
  ((properties_->has_video && properties_->has_audio)       \
       ? (is_muted_ ? WatchTimeKey::kAudioVideoMuted##key   \
                    : WatchTimeKey::kAudioVideo##key)       \
       : properties_->has_audio ? WatchTimeKey::kAudio##key \
                                : WatchTimeKey::kVideo##key)

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreateControlsComponent() {
  DCHECK(!is_background_);

  std::vector<WatchTimeKey> keys_to_finalize{FOREGROUND_KEY(NativeControlsOn),
                                             FOREGROUND_KEY(NativeControlsOff)};

  return std::make_unique<WatchTimeComponent<bool>>(
      false, std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetControlsKey,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

WatchTimeKey WatchTimeReporter::GetControlsKey(bool has_native_controls) {
  return has_native_controls ? FOREGROUND_KEY(NativeControlsOn)
                             : FOREGROUND_KEY(NativeControlsOff);
}

#undef FOREGROUND_KEY

#define DISPLAY_TYPE_KEY(key)                                                \
  (properties_->has_audio ? (is_muted_ ? WatchTimeKey::kAudioVideoMuted##key \
                                       : WatchTimeKey::kAudioVideo##key)     \
                          : WatchTimeKey::kVideo##key)

std::unique_ptr<WatchTimeComponent<WatchTimeReporter::DisplayType>>
WatchTimeReporter::CreateDisplayTypeComponent() {
  DCHECK(properties_->has_video);
  DCHECK(!is_background_);

  std::vector<WatchTimeKey> keys_to_finalize{
      DISPLAY_TYPE_KEY(DisplayInline), DISPLAY_TYPE_KEY(DisplayFullscreen),
      DISPLAY_TYPE_KEY(DisplayPictureInPicture)};

  return std::make_unique<WatchTimeComponent<DisplayType>>(
      DisplayType::kInline, std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetDisplayTypeKey,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

WatchTimeKey WatchTimeReporter::GetDisplayTypeKey(DisplayType display_type) {
  switch (display_type) {
    case DisplayType::kInline:
      return DISPLAY_TYPE_KEY(DisplayInline);
    case DisplayType::kFullscreen:
      return DISPLAY_TYPE_KEY(DisplayFullscreen);
    case DisplayType::kPictureInPicture:
      return DISPLAY_TYPE_KEY(DisplayPictureInPicture);
  }
}

#undef DISPLAY_TYPE_KEY

}  // namespace media
