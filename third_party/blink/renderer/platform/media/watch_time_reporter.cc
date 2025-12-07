// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/watch_time_reporter.h"

#include <numeric>

#include "base/functional/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/watch_time_keys.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// The minimum width and height of videos to report watch time metrics for.
constexpr gfx::Size kMinimumVideoSize = gfx::Size(200, 140);

static bool IsOnBatteryPower() {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  if (!power_monitor->IsInitialized()) {
    return false;
  }
  return power_monitor->GetBatteryPowerStatus() ==
         base::PowerStateObserver::BatteryPowerStatus::kBatteryPower;
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
    media::mojom::blink::PlaybackPropertiesPtr properties,
    const gfx::Size& natural_size,
    GetMediaTimeCB get_media_time_cb,
    GetPipelineStatsCB get_pipeline_stats_cb,
    media::mojom::blink::MediaMetricsProvider* provider,
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
    media::mojom::blink::PlaybackPropertiesPtr properties,
    bool is_background,
    bool is_muted,
    const gfx::Size& natural_size,
    GetMediaTimeCB get_media_time_cb,
    GetPipelineStatsCB get_pipeline_stats_cb,
    media::mojom::blink::MediaMetricsProvider* provider,
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

  base::PowerMonitor::GetInstance()->AddPowerStateObserver(this);

  provider->AcquireWatchTimeRecorder(properties_->Clone(),
                                     recorder_.BindNewPipeAndPassReceiver());

  reporting_timer_.SetTaskRunner(task_runner);

  base_component_ = CreateBaseComponent();
  power_component_ = CreatePowerComponent();
  if (!is_background_) {
    if (properties_->has_video) {
      dominant_component_ = CreateDominantComponent();
    }
    controls_component_ = CreateControlsComponent();
    if (properties_->has_video || properties_->has_audio) {
      display_type_component_ = CreateDisplayTypeComponent();
    }
    if (properties_->has_video && properties_->has_audio && !is_muted_) {
      hdr_component_ = CreateHdrComponent();
    }
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
  base::PowerMonitor::GetInstance()->RemovePowerStateObserver(this);
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

void WatchTimeReporter::OnError(media::PipelineStatus status) {
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
    DCHECK_NE(pending_underflow_events_.back().duration, media::kNoTimestamp);

  // In the event of a pending finalize, we don't want to count underflow events
  // that occurred after the finalize time. Yet if the finalize is canceled we
  // want to ensure they are all recorded.
  pending_underflow_events_.emplace_back(false, get_media_time_cb_.Run(),
                                         media::kNoTimestamp);
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
  DCHECK_EQ(pending_underflow_events_.back().duration, media::kNoTimestamp);
  pending_underflow_events_.back().duration = elapsed;
}

void WatchTimeReporter::OnNativeControlsEnabled() {
  OnNativeControlsChanged(true);
}

void WatchTimeReporter::OnNativeControlsDisabled() {
  OnNativeControlsChanged(false);
}

void WatchTimeReporter::OnDisplayTypeInline() {
  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kInline);
}

void WatchTimeReporter::OnDisplayTypeFullscreen() {
  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kFullscreen);
}

void WatchTimeReporter::OnDisplayTypeVideoPictureInPicture() {
  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kVideoPictureInPicture);
}

void WatchTimeReporter::OnDisplayTypeDocumentPictureInPicture() {
  OnDisplayTypeChanged(WebMediaPlayer::DisplayType::kDocumentPictureInPicture);
}

void WatchTimeReporter::UpdateSecondaryProperties(
    media::mojom::blink::SecondaryPlaybackPropertiesPtr secondary_properties) {
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

void WatchTimeReporter::OnBatteryPowerStatusChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  bool battery_power =
      (battery_power_status ==
       base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  if (HandlePropertyChange<bool>(battery_power, reporting_timer_.IsRunning(),
                                 power_component_.get()) ==
      PropertyAction::kFinalizeRequired) {
    RestartTimerForHysteresis();
  }
}

void WatchTimeReporter::OnDominantVisibleContentChanged(bool is_dominant) {
  if (muted_reporter_) {
    muted_reporter_->OnDominantVisibleContentChanged(is_dominant);
  }

  if (HandlePropertyChange<bool>(is_dominant, reporting_timer_.IsRunning(),
                                 dominant_component_.get()) ==
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

void WatchTimeReporter::OnDisplayTypeChanged(
    WebMediaPlayer::DisplayType display_type) {
  if (muted_reporter_)
    muted_reporter_->OnDisplayTypeChanged(display_type);

  if (HandlePropertyChange<WebMediaPlayer::DisplayType>(
          display_type, reporting_timer_.IsRunning(),
          display_type_component_.get()) == PropertyAction::kFinalizeRequired) {
    RestartTimerForHysteresis();
  }
}

void WatchTimeReporter::OnHdrChanged(bool is_hdr) {
  if (hdr_component_ &&
      HandlePropertyChange<bool>(is_hdr, reporting_timer_.IsRunning(),
                                 hdr_component_.get()) ==
          PropertyAction::kFinalizeRequired) {
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
  has_valid_start_timestamp_ = start_timestamp != media::kInfiniteDuration;

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
    last_stats_ = media::PipelineStatistics();
  }

  ResetUnderflowState();
  base_component_->OnReportingStarted(start_timestamp);
  power_component_->OnReportingStarted(start_timestamp);

  if (controls_component_)
    controls_component_->OnReportingStarted(start_timestamp);
  if (display_type_component_)
    display_type_component_->OnReportingStarted(start_timestamp);
  if (hdr_component_) {
    hdr_component_->OnReportingStarted(start_timestamp);
  }
  if (dominant_component_) {
    dominant_component_->OnReportingStarted(start_timestamp);
  }

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
      constexpr base::TimeDelta kMaximumRebufferDuration = base::Minutes(1);
      if (ufe.duration != media::kNoTimestamp &&
          ufe.duration <= kMaximumRebufferDuration) {
        ++total_completed_underflow_count_;
        total_underflow_duration_ += ufe.duration;
      }
    }

    EraseIf(pending_underflow_events_, [](const UnderflowEvent& ufe) {
      return ufe.reported && ufe.duration != media::kNoTimestamp;
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
  if (hdr_component_) {
    hdr_component_->RecordWatchTime(current_timestamp);
  }
  if (dominant_component_) {
    dominant_component_->RecordWatchTime(current_timestamp);
  }
}

void WatchTimeReporter::UpdateWatchTime() {
  // First record watch time.
  RecordWatchTime();

  // Second, process any pending finalize events.
  Vector<media::WatchTimeKey> keys_to_finalize;
  if (power_component_->NeedsFinalize())
    power_component_->Finalize(&keys_to_finalize);
  if (display_type_component_ && display_type_component_->NeedsFinalize()) {
    display_type_component_->Finalize(&keys_to_finalize);
  }
  if (controls_component_ && controls_component_->NeedsFinalize())
    controls_component_->Finalize(&keys_to_finalize);
  if (hdr_component_ && hdr_component_->NeedsFinalize()) {
    hdr_component_->Finalize(&keys_to_finalize);
  }
  if (dominant_component_ && dominant_component_->NeedsFinalize()) {
    dominant_component_->Finalize(&keys_to_finalize);
  }

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
       ? (is_background_                                                    \
              ? media::WatchTimeKey::kAudioVideoBackground##key             \
              : (is_muted_ ? media::WatchTimeKey::kAudioVideoMuted##key     \
                           : media::WatchTimeKey::kAudioVideo##key))        \
       : properties_->has_video                                             \
             ? (is_background_ ? media::WatchTimeKey::kVideoBackground##key \
                               : media::WatchTimeKey::kVideo##key)          \
             : (is_background_ ? media::WatchTimeKey::kAudioBackground##key \
                               : media::WatchTimeKey::kAudio##key))

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreateBaseComponent() {
  Vector<media::WatchTimeKey> keys_to_finalize;
  keys_to_finalize.emplace_back(NORMAL_KEY(All));

  if (properties_->has_video && properties_->has_audio && !is_background_ &&
      !is_muted_ &&
      properties_->renderer_type == media::RendererType::kMediaFoundation) {
    keys_to_finalize.emplace_back(
        media::WatchTimeKey::kAudioVideoMediaFoundationAll);
    if (properties_->is_eme) {
      keys_to_finalize.emplace_back(
          media::WatchTimeKey::kAudioVideoMediaFoundationEme);
    }
  }

  switch (properties_->demuxer_type) {
    case media::DemuxerType::kMockDemuxer:
    case media::DemuxerType::kUnknownDemuxer:
      // Testing demuxers, do nothing.
      break;
    case media::DemuxerType::kChunkDemuxer:
      keys_to_finalize.emplace_back(NORMAL_KEY(Mse));
      break;
    case media::DemuxerType::kFFmpegDemuxer:
    case media::DemuxerType::kFrameInjectingDemuxer:
    case media::DemuxerType::kStreamProviderDemuxer:
      keys_to_finalize.emplace_back(NORMAL_KEY(Src));
      break;
    case media::DemuxerType::kManifestDemuxer:
      keys_to_finalize.emplace_back(NORMAL_KEY(Hls));
      break;
  }

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
  Vector<media::WatchTimeKey> keys_to_finalize{NORMAL_KEY(Battery),
                                               NORMAL_KEY(Ac)};

  return std::make_unique<WatchTimeComponent<bool>>(
      IsOnBatteryPower(), std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetPowerKeys,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

Vector<media::WatchTimeKey> WatchTimeReporter::GetPowerKeys(
    bool is_on_battery_power) {
  return {is_on_battery_power ? NORMAL_KEY(Battery) : NORMAL_KEY(Ac)};
}
#undef NORMAL_KEY

#define FOREGROUND_VIDEO_KEY(key)                                \
  ((properties_->has_audio)                                      \
       ? (is_muted_ ? media::WatchTimeKey::kAudioVideoMuted##key \
                    : media::WatchTimeKey::kAudioVideo##key)     \
       : media::WatchTimeKey::kVideo##key)

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreateDominantComponent() {
  DCHECK(!is_background_);
  DCHECK(properties_->has_video);

  Vector<media::WatchTimeKey> keys_to_finalize{
      FOREGROUND_VIDEO_KEY(DominantVisibleContent),
      FOREGROUND_VIDEO_KEY(AuxiliaryVisibleContent)};

  return std::make_unique<WatchTimeComponent<bool>>(
      false, std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetDominantKey,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

Vector<media::WatchTimeKey> WatchTimeReporter::GetDominantKey(
    bool is_dominant) {
  return {is_dominant ? FOREGROUND_VIDEO_KEY(DominantVisibleContent)
                      : FOREGROUND_VIDEO_KEY(AuxiliaryVisibleContent)};
}

#undef FOREGROUND_VIDEO_KEY

#define FOREGROUND_KEY(key)                                        \
  ((properties_->has_video && properties_->has_audio)              \
       ? (is_muted_ ? media::WatchTimeKey::kAudioVideoMuted##key   \
                    : media::WatchTimeKey::kAudioVideo##key)       \
       : properties_->has_audio ? media::WatchTimeKey::kAudio##key \
                                : media::WatchTimeKey::kVideo##key)

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreateControlsComponent() {
  DCHECK(!is_background_);

  Vector<media::WatchTimeKey> keys_to_finalize{
      FOREGROUND_KEY(NativeControlsOn), FOREGROUND_KEY(NativeControlsOff)};

  return std::make_unique<WatchTimeComponent<bool>>(
      false, std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetControlsKeys,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

Vector<media::WatchTimeKey> WatchTimeReporter::GetControlsKeys(
    bool has_native_controls) {
  return {has_native_controls ? FOREGROUND_KEY(NativeControlsOn)
                              : FOREGROUND_KEY(NativeControlsOff)};
}

#undef FOREGROUND_KEY

#define DISPLAY_TYPE_KEY(key)                                    \
  ((properties_->has_audio && properties_->has_video)            \
       ? (is_muted_ ? media::WatchTimeKey::kAudioVideoMuted##key \
                    : media::WatchTimeKey::kAudioVideo##key)     \
   : properties_->has_audio ? media::WatchTimeKey::kAudio##key   \
                            : media::WatchTimeKey::kVideo##key)

std::unique_ptr<WatchTimeComponent<WebMediaPlayer::DisplayType>>
WatchTimeReporter::CreateDisplayTypeComponent() {
  DCHECK(properties_->has_video || properties_->has_audio);
  DCHECK(!is_background_);

  Vector<media::WatchTimeKey> keys_to_finalize{
      DISPLAY_TYPE_KEY(DisplayInline), DISPLAY_TYPE_KEY(DisplayFullscreen),
      DISPLAY_TYPE_KEY(DisplayPictureInPicture)};

  if (properties_->has_audio && properties_->has_video) {
    keys_to_finalize.emplace_back(
        media::WatchTimeKey::kAudioVideoAutoPipMediaPlayback);
  }

  if (properties_->has_audio && !properties_->has_video) {
    keys_to_finalize.emplace_back(
        media::WatchTimeKey::kAudioAutoPipMediaPlayback);
  }

  return std::make_unique<WatchTimeComponent<WebMediaPlayer::DisplayType>>(
      WebMediaPlayer::DisplayType::kInline, std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetDisplayTypeKeys,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

Vector<media::WatchTimeKey> WatchTimeReporter::GetDisplayTypeKeys(
    WebMediaPlayer::DisplayType display_type) {
  switch (display_type) {
    case WebMediaPlayer::DisplayType::kInline:
      return {DISPLAY_TYPE_KEY(DisplayInline)};
    case WebMediaPlayer::DisplayType::kFullscreen:
      return {DISPLAY_TYPE_KEY(DisplayFullscreen)};
    case WebMediaPlayer::DisplayType::kVideoPictureInPicture:
    case WebMediaPlayer::DisplayType::kDocumentPictureInPicture:
      return {DISPLAY_TYPE_KEY(DisplayPictureInPicture)};
  }
}

#undef DISPLAY_TYPE_KEY

#define HDR_KEY(is_eme, is_hdr)                              \
  is_hdr ? (is_eme ? media::WatchTimeKey::kAudioVideoHdrEme  \
                   : media::WatchTimeKey::kAudioVideoHdrAll) \
         : (is_eme ? media::WatchTimeKey::kAudioVideoSdrEme  \
                   : media::WatchTimeKey::kAudioVideoSdrAll)

#define MEDIA_FOUNDATION_HDR_KEY(is_eme, is_hdr)                            \
  is_hdr ? (is_eme ? media::WatchTimeKey::kAudioVideoMediaFoundationHdrEme  \
                   : media::WatchTimeKey::kAudioVideoMediaFoundationHdrAll) \
         : (is_eme ? media::WatchTimeKey::kAudioVideoMediaFoundationSdrEme  \
                   : media::WatchTimeKey::kAudioVideoMediaFoundationSdrAll)

std::unique_ptr<WatchTimeComponent<bool>>
WatchTimeReporter::CreateHdrComponent() {
  Vector<media::WatchTimeKey> keys_to_finalize{HDR_KEY(false, true),
                                               HDR_KEY(false, false)};
  const auto is_media_foundation =
      properties_->renderer_type == media::RendererType::kMediaFoundation;
  if (is_media_foundation) {
    keys_to_finalize.emplace_back(MEDIA_FOUNDATION_HDR_KEY(false, true));
    keys_to_finalize.emplace_back(MEDIA_FOUNDATION_HDR_KEY(false, false));
  }
  if (properties_->is_eme) {
    if (is_media_foundation) {
      keys_to_finalize.emplace_back(MEDIA_FOUNDATION_HDR_KEY(true, true));
      keys_to_finalize.emplace_back(MEDIA_FOUNDATION_HDR_KEY(true, false));
    } else {
      keys_to_finalize.emplace_back(HDR_KEY(true, true));
      keys_to_finalize.emplace_back(HDR_KEY(true, false));
    }
  }

  return std::make_unique<WatchTimeComponent<bool>>(
      false, std::move(keys_to_finalize),
      base::BindRepeating(&WatchTimeReporter::GetHdrKeys,
                          base::Unretained(this)),
      get_media_time_cb_, recorder_.get());
}

Vector<media::WatchTimeKey> WatchTimeReporter::GetHdrKeys(bool is_hdr) {
  const auto is_media_foundation =
      properties_->renderer_type == media::RendererType::kMediaFoundation;
  if (properties_->is_eme) {
    if (is_media_foundation) {
      return {HDR_KEY(false, is_hdr), HDR_KEY(true, is_hdr),
              MEDIA_FOUNDATION_HDR_KEY(false, is_hdr),
              MEDIA_FOUNDATION_HDR_KEY(true, is_hdr)};
    }
    return {HDR_KEY(false, is_hdr), HDR_KEY(true, is_hdr)};
  }

  if (is_media_foundation) {
    return {HDR_KEY(false, is_hdr), MEDIA_FOUNDATION_HDR_KEY(false, is_hdr)};
  }
  return {HDR_KEY(false, is_hdr)};
}

#undef HDR_KEY

}  // namespace blink
