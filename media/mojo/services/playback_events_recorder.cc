// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/playback_events_recorder.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

namespace {

void RecordEventWithValueAt(const char* name,
                            int64_t value,
                            base::TimeTicks time) {
  base::RecordComputedActionAt(base::StrCat({"WebEngine.Media.", name, ":",
                                             base::NumberToString(value)}),
                               time);
}

void RecordEventWithValue(const char* name, int64_t value) {
  RecordEventWithValueAt(name, value, base::TimeTicks::Now());
}

constexpr base::TimeDelta kBitrateReportPeriod = base::Seconds(5);

}  // namespace

PlaybackEventsRecorder::BitrateEstimator::BitrateEstimator() {}
PlaybackEventsRecorder::BitrateEstimator::~BitrateEstimator() {}

void PlaybackEventsRecorder::BitrateEstimator::Update(
    const PipelineStatistics& stats) {
  base::TimeTicks now = base::TimeTicks::Now();

  // The code below trusts that |stats| are valid even though they came from an
  // untrusted process. That's accepable because the stats are used only to
  // record metrics.
  if (last_stats_) {
    time_elapsed_ += now - last_stats_time_;
    audio_bytes_ +=
        stats.audio_bytes_decoded - last_stats_->audio_bytes_decoded;
    video_bytes_ +=
        stats.video_bytes_decoded - last_stats_->video_bytes_decoded;
    if (time_elapsed_ >= kBitrateReportPeriod) {
      size_t audio_bitrate_kbps =
          8 * audio_bytes_ / time_elapsed_.InMilliseconds();
      RecordEventWithValueAt("AudioBitrate", audio_bitrate_kbps, now);

      size_t video_bitrate_kbps =
          8 * video_bytes_ / time_elapsed_.InMilliseconds();
      RecordEventWithValueAt("VideoBitrate", video_bitrate_kbps, now);

      time_elapsed_ = base::TimeDelta();
      audio_bytes_ = 0;
      video_bytes_ = 0;
    }
  }

  last_stats_ = stats;
  last_stats_time_ = now;
}

void PlaybackEventsRecorder::BitrateEstimator::OnPause() {
  last_stats_ = {};
}

// static
void PlaybackEventsRecorder::Create(
    mojo::PendingReceiver<mojom::PlaybackEventsRecorder> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<PlaybackEventsRecorder>(),
                              std::move(receiver));
}

PlaybackEventsRecorder::PlaybackEventsRecorder() = default;
PlaybackEventsRecorder::~PlaybackEventsRecorder() = default;

void PlaybackEventsRecorder::OnPlaying() {
  base::RecordComputedAction("WebEngine.Media.Playing");
}

void PlaybackEventsRecorder::OnPaused() {
  base::RecordComputedAction("WebEngine.Media.Pause");
  bitrate_estimator_.OnPause();
}

void PlaybackEventsRecorder::OnSeeking() {
  buffering_state_ = BufferingState::kInitialBuffering;
}

void PlaybackEventsRecorder::OnEnded() {
  base::RecordComputedAction("WebEngine.Media.Ended");
}

void PlaybackEventsRecorder::OnBuffering() {
  DCHECK(buffering_state_ == BufferingState::kBuffered);

  buffering_start_time_ = base::TimeTicks::Now();
  buffering_state_ = BufferingState::kBuffering;

  bitrate_estimator_.OnPause();
}

void PlaybackEventsRecorder::OnBufferingComplete() {
  auto now = base::TimeTicks::Now();

  if (buffering_state_ == BufferingState::kBuffering) {
    base::TimeDelta time_between_buffering =
        buffering_start_time_ - last_buffering_end_time_;
    RecordEventWithValueAt("PlayTimeBeforeAutoPause",
                           time_between_buffering.InMilliseconds(), now);

    base::TimeDelta buffering_user_time = now - buffering_start_time_;
    RecordEventWithValueAt("AutoPauseTime",
                           buffering_user_time.InMilliseconds(), now);
  }

  buffering_state_ = BufferingState::kBuffered;
  last_buffering_end_time_ = now;
}

void PlaybackEventsRecorder::OnError(const PipelineStatus& status) {
  RecordEventWithValue("Error", status.code());
}

void PlaybackEventsRecorder::OnNaturalSizeChanged(const gfx::Size& size) {
  int encoded_video_resolution = (size.width() << 16) | size.height();
  base::RecordComputedAction(base::StringPrintf(
      "WebEngine.Media.VideoResolution:%d", encoded_video_resolution));
}

void PlaybackEventsRecorder::OnPipelineStatistics(
    const PipelineStatistics& stats) {
  bitrate_estimator_.Update(stats);
}

}  // namespace media
