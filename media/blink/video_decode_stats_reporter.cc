// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/video_decode_stats_reporter.h"

#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/macros.h"
#include "media/capabilities/bucket_utility.h"

namespace media {

VideoDecodeStatsReporter::VideoDecodeStatsReporter(
    mojom::VideoDecodeStatsRecorderPtr recorder_ptr,
    GetPipelineStatsCB get_pipeline_stats_cb,
    const VideoDecoderConfig& video_config,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock)
    : kRecordingInterval(
          base::TimeDelta::FromMilliseconds(kRecordingIntervalMs)),
      kTinyFpsWindowDuration(
          base::TimeDelta::FromMilliseconds(kTinyFpsWindowMs)),
      recorder_ptr_(std::move(recorder_ptr)),
      get_pipeline_stats_cb_(std::move(get_pipeline_stats_cb)),
      video_config_(video_config),
      natural_size_(GetSizeBucket(video_config.natural_size())),
      tick_clock_(tick_clock),
      stats_cb_timer_(tick_clock_) {
  DCHECK(recorder_ptr_.is_bound());
  DCHECK(get_pipeline_stats_cb_);
  DCHECK(video_config_.IsValidConfig());

  recorder_ptr_.set_connection_error_handler(base::BindRepeating(
      &VideoDecodeStatsReporter::OnIpcConnectionError, base::Unretained(this)));
  stats_cb_timer_.SetTaskRunner(task_runner);
}

VideoDecodeStatsReporter::~VideoDecodeStatsReporter() = default;

void VideoDecodeStatsReporter::OnPlaying() {
  DVLOG(2) << __func__;

  if (is_playing_)
    return;
  is_playing_ = true;

  DCHECK(!stats_cb_timer_.IsRunning());

  if (ShouldBeReporting()) {
    RunStatsTimerAtInterval(kRecordingInterval);
  }
}

void VideoDecodeStatsReporter::OnPaused() {
  DVLOG(2) << __func__;

  if (!is_playing_)
    return;
  is_playing_ = false;

  // Stop timer until playing resumes.
  stats_cb_timer_.AbandonAndStop();
}

void VideoDecodeStatsReporter::OnNaturalSizeChanged(
    const gfx::Size& natural_size) {
  gfx::Size bucketed_size = GetSizeBucket(natural_size);

  DVLOG(2) << __func__ << " " << natural_size.ToString()
           << " buckets-to:" << bucketed_size.ToString();

  if (bucketed_size == natural_size_)
    return;

  natural_size_ = bucketed_size;

  // We haven't seen any pipeline stats for this size yet, so it may be that
  // the frame rate will change too. Once frame rate stabilized, UpdateStats()
  // will call the recorder_ptr_->StartNewRecord() with this latest size.
  ResetFrameRateState();

  // Bucketed size will be empty for tiny sizes. Stop reporting until the size
  // is larger.
  if (natural_size_.IsEmpty()) {
    DCHECK(!ShouldBeReporting());
    stats_cb_timer_.AbandonAndStop();

    // Consider starting the timer. Natural size may now be large enough to
    // begin reporting. Also, resetting frame rate state above may give us
    // another chance at frame rate detection if past attempts failed. Avoid
    // calling if already running, as this will increase the delay for the next
    // UpdateStats() callback.
  } else if (ShouldBeReporting() && !stats_cb_timer_.IsRunning()) {
    RunStatsTimerAtInterval(kRecordingInterval);
  }
}

void VideoDecodeStatsReporter::OnVideoConfigChanged(
    const VideoDecoderConfig& video_config) {
  DVLOG(2) << __func__ << " " << video_config.AsHumanReadableString();

  if (video_config.Matches(video_config_))
    return;

  video_config_ = video_config;
  natural_size_ = GetSizeBucket(video_config.natural_size());

  // Force frame rate detection for the new config. Once frame rate stabilized,
  // UpdateStats() will call the recorder_ptr_->StartNewRecord() with this
  // latest config.
  ResetFrameRateState();

  // Resetting the frame rate state may allow us to begin reporting again. Also,
  // if the timer is already running we still want to reset the timer to give
  // the pipeline a chance to stabilize. Config changes may trigger little
  // hiccups while the decoder is reset.
  if (ShouldBeReporting())
    RunStatsTimerAtInterval(kRecordingInterval);
}

void VideoDecodeStatsReporter::OnHidden() {
  DVLOG(2) << __func__;

  if (is_backgrounded_)
    return;

  is_backgrounded_ = true;

  // Stop timer until no longer hidden.
  stats_cb_timer_.AbandonAndStop();
}

void VideoDecodeStatsReporter::OnShown() {
  DVLOG(2) << __func__;

  if (!is_backgrounded_)
    return;

  is_backgrounded_ = false;

  // Only start a new record below if stable FPS has been detected. If FPS is
  // later detected, a new record will be started at that time.
  if (num_stable_fps_samples_ >= kRequiredStableFpsSamples) {
    // Dropped frames are not reported during background rendering. Start a new
    // record to avoid reporting background stats.
    PipelineStatistics stats = get_pipeline_stats_cb_.Run();
    StartNewRecord(stats.video_frames_decoded, stats.video_frames_dropped,
                   stats.video_frames_decoded_power_efficient);
  }

  if (ShouldBeReporting())
    RunStatsTimerAtInterval(kRecordingInterval);
}

void VideoDecodeStatsReporter::RunStatsTimerAtInterval(
    base::TimeDelta interval) {
  DVLOG(2) << __func__ << " " << interval.InMicroseconds() << " us";
  DCHECK(ShouldBeReporting());

  // NOTE: Avoid optimizing with early returns  if the timer is already running
  // at |milliseconds|. Calling Start below resets the timer clock and some
  // callers (e.g. OnVideoConfigChanged) rely on that behavior behavior.
  stats_cb_timer_.Start(FROM_HERE, interval, this,
                        &VideoDecodeStatsReporter::UpdateStats);
}

void VideoDecodeStatsReporter::StartNewRecord(
    uint32_t frames_decoded_offset,
    uint32_t frames_dropped_offset,
    uint32_t frames_decoded_power_efficient_offset) {
  DVLOG(2) << __func__ << " "
           << " profile:" << video_config_.profile()
           << " fps:" << last_observed_fps_
           << " size:" << natural_size_.ToString();

  // Size and frame rate should always be bucketed.
  DCHECK(natural_size_ == GetSizeBucket(natural_size_));
  DCHECK_EQ(last_observed_fps_, GetFpsBucket(last_observed_fps_));

  // New records decoded and dropped counts should start at zero.
  // These should never move backward.
  DCHECK_GE(frames_decoded_offset, frames_decoded_offset_);
  DCHECK_GE(frames_dropped_offset, frames_dropped_offset_);
  DCHECK_GE(frames_decoded_power_efficient_offset,
            frames_decoded_power_efficient_offset_);
  frames_decoded_offset_ = frames_decoded_offset;
  frames_dropped_offset_ = frames_dropped_offset;
  frames_decoded_power_efficient_offset_ =
      frames_decoded_power_efficient_offset;
  mojom::PredictionFeaturesPtr features = mojom::PredictionFeatures::New(
      video_config_.profile(), natural_size_, last_observed_fps_);
  recorder_ptr_->StartNewRecord(std::move(features));
}

void VideoDecodeStatsReporter::ResetFrameRateState() {
  // Reinitialize all frame rate state. The next UpdateStats() call will detect
  // the frame rate.
  last_observed_fps_ = 0;
  num_stable_fps_samples_ = 0;
  num_unstable_fps_changes_ = 0;
  num_consecutive_tiny_fps_windows_ = 0;
  fps_stabilization_failed_ = false;
  last_fps_stabilized_ticks_ = base::TimeTicks();
}

bool VideoDecodeStatsReporter::ShouldBeReporting() const {
  return is_playing_ && !is_backgrounded_ && !fps_stabilization_failed_ &&
         !natural_size_.IsEmpty() && is_ipc_connected_;
}

void VideoDecodeStatsReporter::OnIpcConnectionError() {
  // For incognito, the IPC will fail via this path because the recording
  // service is unavailable. Otherwise, errors are unexpected.
  DVLOG(2) << __func__ << " IPC disconnected. Stopping reporting.";
  is_ipc_connected_ = false;
  stats_cb_timer_.AbandonAndStop();
}

bool VideoDecodeStatsReporter::UpdateDecodeProgress(
    const PipelineStatistics& stats) {
  DCHECK_GE(stats.video_frames_decoded, last_frames_decoded_);
  DCHECK_GE(stats.video_frames_dropped, last_frames_dropped_);
  DCHECK_GE(stats.video_frames_decoded, stats.video_frames_dropped);

  // Check if additional frames decoded since last stats update.
  if (stats.video_frames_decoded == last_frames_decoded_) {
    // Relax timer if its set to a short interval for frame rate stabilization.
    if (stats_cb_timer_.GetCurrentDelay() < kRecordingInterval) {
      DVLOG(2) << __func__ << " No decode progress; slowing the timer";
      RunStatsTimerAtInterval(kRecordingInterval);
    }
    return false;
  }

  last_frames_decoded_ = stats.video_frames_decoded;
  last_frames_dropped_ = stats.video_frames_dropped;

  return true;
}

bool VideoDecodeStatsReporter::UpdateFrameRateStability(
    const PipelineStatistics& stats) {
  // When (re)initializing, the pipeline may momentarily return an average frame
  // duration of zero. Ignore it and wait for a real frame rate.
  if (stats.video_frame_duration_average.is_zero())
    return false;

  // Bucket frame rate to simplify metrics aggregation.
  int frame_rate =
      GetFpsBucket(1 / stats.video_frame_duration_average.InSecondsF());

  if (frame_rate != last_observed_fps_) {
    DVLOG(2) << __func__ << " fps changed: " << last_observed_fps_ << " -> "
             << frame_rate;
    last_observed_fps_ = frame_rate;
    bool was_stable = num_stable_fps_samples_ >= kRequiredStableFpsSamples;
    num_stable_fps_samples_ = 1;
    num_unstable_fps_changes_++;

    // FrameRate just destabilized. Check if last stability window was "tiny".
    if (was_stable) {
      if (tick_clock_->NowTicks() - last_fps_stabilized_ticks_ <
          kTinyFpsWindowDuration) {
        num_consecutive_tiny_fps_windows_++;
        DVLOG(2) << __func__ << " Last FPS window was 'tiny'. num_tiny:"
                 << num_consecutive_tiny_fps_windows_;

        // Stop reporting if FPS moves around a lot. Stats may be noisy.
        if (num_consecutive_tiny_fps_windows_ >= kMaxTinyFpsWindows) {
          DVLOG(2) << __func__ << " Too many tiny fps windows. Stopping timer";
          fps_stabilization_failed_ = true;
          stats_cb_timer_.AbandonAndStop();
          return false;
        }
      } else {
        num_consecutive_tiny_fps_windows_ = 0;
      }
    }

    if (num_unstable_fps_changes_ >= kMaxUnstableFpsChanges) {
      // Looks like VFR video. Wait for some stream property (e.g. decoder
      // config) to change before trying again.
      DVLOG(2) << __func__ << " Unable to stabilize FPS. Stopping timer.";
      fps_stabilization_failed_ = true;
      stats_cb_timer_.AbandonAndStop();
      return false;
    }

    // Increase the timer frequency to quickly stabilize frame rate. 3x the
    // frame duration is used as this should be enough for a few more frames to
    // be decoded, while also being much faster (for typical frame rates) than
    // the regular stats polling interval.
    RunStatsTimerAtInterval(3 * stats.video_frame_duration_average);
    return false;
  }

  // FrameRate matched last observed!
  num_unstable_fps_changes_ = 0;
  num_stable_fps_samples_++;

  // Wait for steady frame rate to begin recording stats.
  if (num_stable_fps_samples_ < kRequiredStableFpsSamples) {
    DVLOG(2) << __func__ << " fps held, awaiting stable ("
             << num_stable_fps_samples_ << ")";
    return false;
  } else if (num_stable_fps_samples_ == kRequiredStableFpsSamples) {
    DVLOG(2) << __func__ << " fps stabilized at " << frame_rate;
    last_fps_stabilized_ticks_ = tick_clock_->NowTicks();

    // FPS is locked in. Start a new record, and set timer to reporting
    // interval.
    StartNewRecord(stats.video_frames_decoded, stats.video_frames_dropped,
                   stats.video_frames_decoded_power_efficient);
    RunStatsTimerAtInterval(kRecordingInterval);
  }
  return true;
}

void VideoDecodeStatsReporter::UpdateStats() {
  DCHECK(ShouldBeReporting());

  PipelineStatistics stats = get_pipeline_stats_cb_.Run();
  DVLOG(2) << __func__ << " Raw stats -- dropped:" << stats.video_frames_dropped
           << "/" << stats.video_frames_decoded
           << " power efficient:" << stats.video_frames_decoded_power_efficient
           << "/" << stats.video_frames_decoded
           << " dur_avg:" << stats.video_frame_duration_average;

  // Evaluate decode progress and update various internal state. Bail if decode
  // is not progressing.
  if (!UpdateDecodeProgress(stats))
    return;

  // Check frame rate for changes. Bail if frame rate needs more samples to
  // stabilize.
  if (!UpdateFrameRateStability(stats))
    return;

  // Don't bother recording the first record immediately after stabilization.
  // Counts of zero don't add value.
  if (stats.video_frames_decoded == frames_decoded_offset_)
    return;

  mojom::PredictionTargetsPtr targets = mojom::PredictionTargets::New(
      stats.video_frames_decoded - frames_decoded_offset_,
      stats.video_frames_dropped - frames_dropped_offset_,
      stats.video_frames_decoded_power_efficient -
          frames_decoded_power_efficient_offset_);

  DVLOG(2) << __func__ << " Recording -- dropped:" << targets->frames_dropped
           << "/" << targets->frames_decoded
           << " power efficient:" << targets->frames_power_efficient << "/"
           << targets->frames_decoded;
  recorder_ptr_->UpdateRecord(std::move(targets));
}

}  // namespace media
