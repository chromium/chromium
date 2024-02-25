// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_renderer_algorithm.h"

#include <limits>

#include "base/ranges/algorithm.h"
#include "media/base/media_log.h"

namespace media {

const int kMaxOutOfOrderFrameLogs = 10;

VideoRendererAlgorithm::ReadyFrame::ReadyFrame(
    scoped_refptr<VideoFrame> ready_frame)
    : frame(std::move(ready_frame)),
      has_estimated_end_time(true),
      ideal_render_count(0),
      render_count(0),
      drop_count(0) {}

VideoRendererAlgorithm::ReadyFrame::ReadyFrame(const ReadyFrame& other) =
    default;

VideoRendererAlgorithm::ReadyFrame::~ReadyFrame() = default;

bool VideoRendererAlgorithm::ReadyFrame::operator<(
    const ReadyFrame& other) const {
  return frame->timestamp() < other.frame->timestamp();
}

VideoRendererAlgorithm::VideoRendererAlgorithm(
    const TimeSource::WallClockTimeCB& wall_clock_time_cb,
    MediaLog* media_log)
    : media_log_(media_log),
      cadence_estimator_(
          base::Seconds(kMinimumAcceptableTimeBetweenGlitchesSecs)),
      wall_clock_time_cb_(wall_clock_time_cb),
      frame_duration_calculator_(kMovingAverageSamples),
      frame_dropping_disabled_(false) {
  DCHECK(wall_clock_time_cb_);
  Reset();
}

VideoRendererAlgorithm::~VideoRendererAlgorithm() = default;

scoped_refptr<VideoFrame> VideoRendererAlgorithm::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    size_t* frames_dropped) {
  DCHECK_LE(deadline_min, deadline_max);

  if (frame_queue_.empty())
    return nullptr;

  if (frames_dropped)
    *frames_dropped = frames_dropped_during_enqueue_;
  frames_dropped_during_enqueue_ = 0;
  have_rendered_frames_ = true;

  // Step 1: Update the current render interval for subroutines.
  render_interval_ = deadline_max - deadline_min;

  // Step 2: Figure out if any intervals have been skipped since the last call
  // to Render().  If so, we assume the last frame provided was rendered during
  // those intervals and adjust its render count appropriately.
  AccountForMissedIntervals(deadline_min, deadline_max);

  // Step 3: Update the wall clock timestamps and frame duration estimates for
  // all frames currently in the |frame_queue_|.
  UpdateFrameStatistics();
  const bool have_known_duration = average_frame_duration_.is_positive();
  if (!was_time_moving_ || !have_known_duration || render_interval_.is_zero()) {
    ReadyFrame& ready_frame = frame_queue_.front();
    DCHECK(ready_frame.frame);
    ++ready_frame.render_count;
    UpdateEffectiveFramesQueued();

    // If time stops, we should reset the |first_frame_| marker.
    if (!was_time_moving_)
      first_frame_ = true;
    return ready_frame.frame;
  }

  last_deadline_max_ = deadline_max;
  base::TimeDelta selected_frame_drift, cadence_frame_drift;

  // Step 4: Attempt to find the best frame by cadence.
  const int cadence_frame = FindBestFrameByCadence();
  int frame_to_render = cadence_frame;
  if (frame_to_render >= 0) {
    cadence_frame_drift = selected_frame_drift =
        CalculateAbsoluteDriftForFrame(deadline_min, frame_to_render);
  }

  // Step 5: If no frame could be found by cadence or the selected frame exceeds
  // acceptable drift, try to find the best frame by coverage of the deadline.
  if (frame_to_render < 0 || selected_frame_drift > max_acceptable_drift_) {
    int second_best_by_coverage = -1;
    const int best_by_coverage = FindBestFrameByCoverage(
        deadline_min, deadline_max, &second_best_by_coverage);

    // If the frame was previously selected based on cadence, we're only here
    // because the drift is too large, so even if the cadence frame has the best
    // coverage, fallback to the second best by coverage if it has better drift.
    if (frame_to_render == best_by_coverage && second_best_by_coverage >= 0 &&
        CalculateAbsoluteDriftForFrame(deadline_min, second_best_by_coverage) <=
            selected_frame_drift) {
      frame_to_render = second_best_by_coverage;
    } else {
      frame_to_render = best_by_coverage;
    }

    if (frame_to_render >= 0) {
      selected_frame_drift =
          CalculateAbsoluteDriftForFrame(deadline_min, frame_to_render);
    }
  }

  // Step 6: If _still_ no frame could be found by coverage, try to choose the
  // least crappy option based on the drift from the deadline. If we're here the
  // selection is going to be bad because it means no suitable frame has any
  // coverage of the deadline interval.
  if (frame_to_render < 0 || selected_frame_drift > max_acceptable_drift_)
    frame_to_render = FindBestFrameByDrift(deadline_min, &selected_frame_drift);

  const bool ignored_cadence_frame =
      cadence_frame >= 0 && frame_to_render != cadence_frame;
  if (ignored_cadence_frame) {
    DVLOG(2) << "Cadence frame "
             << frame_queue_[cadence_frame].frame->timestamp() << " ("
             << cadence_frame << ") overridden by drift: "
             << cadence_frame_drift.InMillisecondsF() << "ms, using "
             << frame_queue_[frame_to_render].frame->timestamp() << "("
             << frame_to_render << ") instead.";
  }

  last_render_had_glitch_ = selected_frame_drift > max_acceptable_drift_;
  DVLOG_IF(2, last_render_had_glitch_)
      << "Frame drift is too far: " << selected_frame_drift.InMillisecondsF()
      << "ms";

  DCHECK_GE(frame_to_render, 0);

  // Drop some debugging information if a frame had poor cadence.
  if (cadence_estimator_.has_cadence()) {
    const ReadyFrame& last_frame_info = frame_queue_.front();
    if (frame_to_render &&
        last_frame_info.render_count < last_frame_info.ideal_render_count) {
      last_render_had_glitch_ = true;
      DVLOG(2) << "Under-rendered frame " << last_frame_info.frame->timestamp()
               << "; only " << last_frame_info.render_count
               << " times instead of " << last_frame_info.ideal_render_count;
    } else if (!frame_to_render &&
               last_frame_info.render_count >=
                   last_frame_info.ideal_render_count) {
      DVLOG(2) << "Over-rendered frame " << last_frame_info.frame->timestamp()
               << "; rendered " << last_frame_info.render_count + 1
               << " times instead of " << last_frame_info.ideal_render_count;
      last_render_had_glitch_ = true;
    }
  }

  // Step 7: Drop frames which occur prior to the frame to be rendered. If any
  // frame unexpectedly has a zero render count it should be reported as
  // dropped. When using cadence some frames may be expected to be skipped and
  // should not be counted as dropped.
  if (frame_to_render > 0) {
    if (frames_dropped) {
      for (int i = 0; i < frame_to_render; ++i) {
        const ReadyFrame& frame = frame_queue_[i];

        // If a frame was ever rendered, don't count it as dropped.
        if (frame.render_count != frame.drop_count)
          continue;

        // If we expected to never render the frame, don't count it as dropped.
        if (cadence_estimator_.has_cadence() && !frame.ideal_render_count)
          continue;

        // If frame dropping is disabled, ignore the results of the algorithm
        // and return the earliest unrendered frame.
        if (frame_dropping_disabled_) {
          frame_to_render = i;
          break;
        }

        DVLOG(2) << "Dropping unrendered (or always dropped) frame "
                 << frame.frame->timestamp()
                 << ", wall clock: " << frame.start_time.ToInternalValue()
                 << " (" << frame.render_count << ", " << frame.drop_count
                 << ")";
        ++(*frames_dropped);
        if (!cadence_estimator_.has_cadence() || frame.ideal_render_count)
          last_render_had_glitch_ = true;
      }
    }

    // Increment the frame counter for all frames removed after the last
    // rendered frame.
    cadence_frame_counter_ += frame_to_render;
    frame_queue_.erase(frame_queue_.begin(),
                       frame_queue_.begin() + frame_to_render);
  }

  if (last_render_had_glitch_ && !first_frame_) {
    DVLOG(2) << "Deadline: [" << deadline_min.ToInternalValue() << ", "
             << deadline_max.ToInternalValue()
             << "], Interval: " << render_interval_.InMicroseconds()
             << ", Duration: " << average_frame_duration_.InMicroseconds();
  }

  // Step 8: Congratulations, the frame selection gauntlet has been passed!
  if (first_frame_ && frame_to_render > 0)
    first_frame_ = false;

  ++frame_queue_.front().render_count;

  // Once we reach a glitch in our cadence sequence, reset the base frame number
  // used for defining the cadence sequence; the sequence restarts from the
  // selected frame.
  if (ignored_cadence_frame) {
    cadence_frame_counter_ = 0;
    UpdateCadenceForFrames();
  }

  UpdateEffectiveFramesQueued();
  DCHECK(frame_queue_.front().frame);
  return frame_queue_.front().frame;
}

size_t VideoRendererAlgorithm::RemoveExpiredFrames(base::TimeTicks deadline) {
  // Update |last_deadline_max_| if it's no longer accurate; this should always
  // be done or EffectiveFramesQueued() may never expire the last frame.
  if (deadline > last_deadline_max_)
    last_deadline_max_ = deadline;

  if (frame_queue_.empty())
    return 0;

  // Even though we may not be able to remove anything due to having only one
  // frame, correct any estimates which may have been set during EnqueueFrame().
  UpdateFrameStatistics();
  UpdateEffectiveFramesQueued();

  // We always leave at least one frame in the queue, so if there's only one
  // frame there's nothing we can expire.
  if (frame_queue_.size() == 1)
    return 0;

  DCHECK_GT(average_frame_duration_, base::TimeDelta());

  // Expire everything before the first good frame or everything but the last
  // frame if there is no good frame.
  const int first_good_frame = FindFirstGoodFrame();
  const size_t frames_to_expire =
      first_good_frame < 0 ? frame_queue_.size() - 1 : first_good_frame;
  if (!frames_to_expire)
    return 0;

  size_t frames_dropped_without_rendering = 0;
  for (size_t i = 0; i < frames_to_expire; ++i) {
    const ReadyFrame& frame = frame_queue_[i];

    // Don't count frames that are intentionally dropped by cadence as dropped.
    if (frame.render_count == frame.drop_count &&
        (!cadence_estimator_.has_cadence() || frame.ideal_render_count)) {
      ++frames_dropped_without_rendering;
    }
  }

  cadence_frame_counter_ += frames_to_expire;
  frame_queue_.erase(frame_queue_.begin(),
                     frame_queue_.begin() + frames_to_expire);
  return frames_dropped_without_rendering;
}

void VideoRendererAlgorithm::OnLastFrameDropped() {
  // Since compositing is disconnected from the algorithm, the algorithm may be
  // Reset() in between ticks of the compositor, so discard notifications which
  // are invalid.
  if (!have_rendered_frames_ || frame_queue_.empty())
    return;

  // If frames were expired by RemoveExpiredFrames() this count may be zero when
  // the OnLastFrameDropped() call comes in.
  ReadyFrame& frame = frame_queue_.front();
  if (!frame.render_count)
    return;

  ++frame.drop_count;
  DCHECK_LE(frame.drop_count, frame.render_count);
  UpdateEffectiveFramesQueued();
}

void VideoRendererAlgorithm::Reset(ResetFlag reset_flag) {
  out_of_order_frame_logs_ = 0;
  frames_dropped_during_enqueue_ = 0;
  have_rendered_frames_ = last_render_had_glitch_ = false;
  render_interval_ = base::TimeDelta();
  frame_queue_.clear();
  cadence_estimator_.Reset();
  if (reset_flag != ResetFlag::kPreserveNextFrameEstimates) {
    average_frame_duration_ = base::TimeDelta();
    last_deadline_max_ = base::TimeTicks();
    frame_duration_calculator_.Reset();
  }
  first_frame_ = true;
  effective_frames_queued_ = cadence_frame_counter_ = 0;
  was_time_moving_ = false;

  // Default to ATSC IS/191 recommendations for maximum acceptable drift before
  // we have enough frames to base the maximum on frame duration.
  max_acceptable_drift_ = base::Milliseconds(15);
}

int64_t VideoRendererAlgorithm::GetMemoryUsage() const {
  int64_t allocation_size = 0;
  for (const auto& ready_frame : frame_queue_) {
    allocation_size += VideoFrame::AllocationSize(
        ready_frame.frame->format(), ready_frame.frame->coded_size());
  }
  return allocation_size;
}

void VideoRendererAlgorithm::EnqueueFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK(frame);
  DCHECK(!frame->metadata().end_of_stream);

  // Note: Not all frames have duration. E.g., this class is used with WebRTC
  // which does not provide duration information for its frames.
  base::TimeDelta metadata_frame_duration =
      frame->metadata().frame_duration.value_or(base::TimeDelta());
  auto timestamp = frame->timestamp();
  ReadyFrame ready_frame(std::move(frame));
  auto it = frame_queue_.empty()
                ? frame_queue_.end()
                : std::lower_bound(frame_queue_.begin(), frame_queue_.end(),
                                   ready_frame);
  DCHECK_GE(it - frame_queue_.begin(), 0);

  // Drop any frames inserted before or at the last rendered frame if we've
  // already rendered any frames.
  const size_t new_frame_index = it - frame_queue_.begin();
  if (new_frame_index <= 0 && have_rendered_frames_) {
    LIMITED_MEDIA_LOG(INFO, media_log_, out_of_order_frame_logs_,
                      kMaxOutOfOrderFrameLogs)
        << "Dropping frame with timestamp " << timestamp
        << ", which is earlier than the last rendered frame ("
        << frame_queue_.front().frame->timestamp() << ").";
    ++frames_dropped_during_enqueue_;
    return;
  }

  // Drop any frames which are less than a millisecond apart in media time (even
  // those with timestamps matching an already enqueued frame), there's no way
  // we can reasonably render these frames; it's effectively a 1000fps limit.
  const base::TimeDelta delta = std::min(
      new_frame_index < frame_queue_.size()
          ? frame_queue_[new_frame_index].frame->timestamp() - timestamp
          : base::TimeDelta::Max(),
      new_frame_index > 0
          ? timestamp - frame_queue_[new_frame_index - 1].frame->timestamp()
          : base::TimeDelta::Max());
  if (delta < base::Milliseconds(1)) {
    DVLOG(2) << "Dropping frame too close to an already enqueued frame: "
             << delta.InMicroseconds() << " us";
    ++frames_dropped_during_enqueue_;
    return;
  }

  // Calculate an accurate start time and an estimated end time if possible for
  // the new frame; this allows EffectiveFramesQueued() to be relatively correct
  // immediately after a new frame is queued.
  std::vector<base::TimeDelta> media_timestamps(1, timestamp);

  // If there are not enough frames to estimate duration based on end time, ask
  // the WallClockTimeCB to convert the estimated frame duration into wall clock
  // time.
  //
  // Note: This duration value is not compensated for playback rate and
  // thus is different than |average_frame_duration_| which is compensated.
  if (!frame_duration_calculator_.Count() &&
      metadata_frame_duration.is_positive()) {
    media_timestamps.push_back(timestamp + metadata_frame_duration);
  }

  std::vector<base::TimeTicks> wall_clock_times;
  base::TimeDelta wallclock_duration;
  wall_clock_time_cb_.Run(media_timestamps, &wall_clock_times);
  ready_frame.start_time = wall_clock_times[0];
  if (frame_duration_calculator_.Count()) {
    ready_frame.end_time = ready_frame.start_time + average_frame_duration_;
    wallclock_duration = average_frame_duration_;
  } else if (wall_clock_times.size() > 1u) {
    ready_frame.end_time = wall_clock_times[1];
    wallclock_duration = ready_frame.end_time - ready_frame.start_time;
  }

  ready_frame.frame->metadata().wallclock_frame_duration = wallclock_duration;

  // The vast majority of cases should always append to the back, but in rare
  // circumstance we get out of order timestamps, http://crbug.com/386551.
  if (it != frame_queue_.end()) {
    LIMITED_MEDIA_LOG(INFO, media_log_, out_of_order_frame_logs_,
                      kMaxOutOfOrderFrameLogs)
        << "Decoded frame with timestamp " << timestamp << " is out of order.";
  }
  frame_queue_.insert(it, ready_frame);

  // Project the current cadence calculations to include the new frame.  These
  // may not be accurate until the next Render() call.  These updates are done
  // to ensure EffectiveFramesQueued() returns a semi-reliable result.
  if (cadence_estimator_.has_cadence())
    UpdateCadenceForFrames();

  UpdateEffectiveFramesQueued();
#ifndef NDEBUG
  // Verify sorted order in debug mode.
  for (size_t i = 0; i < frame_queue_.size() - 1; ++i) {
    DCHECK(frame_queue_[i].frame->timestamp() <=
           frame_queue_[i + 1].frame->timestamp());
  }
#endif
}

void VideoRendererAlgorithm::AccountForMissedIntervals(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max) {
  if (last_deadline_max_.is_null() || deadline_min <= last_deadline_max_ ||
      !have_rendered_frames_ || !was_time_moving_ ||
      render_interval_.is_zero()) {
    return;
  }

  DCHECK_GT(render_interval_, base::TimeDelta());
  const int64_t render_cycle_count =
      (deadline_min - last_deadline_max_).IntDiv(render_interval_);

  // In the ideal case this value will be zero.
  if (!render_cycle_count)
    return;

  DVLOG(2) << "Missed " << render_cycle_count << " Render() intervals.";

  // Only update render count if the frame was rendered at all; it may not have
  // been if the frame is at the head because we haven't rendered anything yet
  // or because previous frames were removed via RemoveExpiredFrames().
  ReadyFrame& ready_frame = frame_queue_.front();
  if (!ready_frame.render_count)
    return;

  // If the frame was never really rendered since it was dropped each attempt,
  // we need to increase the drop count as well to match the new render count.
  // Otherwise we won't properly count the frame as dropped when it's discarded.
  // We always update the render count so FindBestFrameByCadence() can properly
  // account for potentially over-rendered frames.
  if (ready_frame.render_count == ready_frame.drop_count)
    ready_frame.drop_count += render_cycle_count;
  ready_frame.render_count += render_cycle_count;
}

void VideoRendererAlgorithm::UpdateFrameStatistics() {
  DCHECK(!frame_queue_.empty());

  // Figure out all current ready frame times at once.
  std::vector<base::TimeDelta> media_timestamps;
  media_timestamps.reserve(frame_queue_.size());
  for (const auto& ready_frame : frame_queue_)
    media_timestamps.push_back(ready_frame.frame->timestamp());

  // If available, always use the last frame's metadata duration to estimate the
  // end time for that frame. This is useful when playback ends on long frame
  // duration content.
  //
  // Note: Not all frames have duration. E.g., this class is used with WebRTC
  // which does not provide duration information for its frames.
  bool have_metadata_duration = false;
  {
    const auto& last_frame = frame_queue_.back().frame;
    base::TimeDelta metadata_frame_duration =
        last_frame->metadata().frame_duration.value_or(base::TimeDelta());
    if (metadata_frame_duration.is_positive()) {
      have_metadata_duration = true;
      media_timestamps.push_back(last_frame->timestamp() +
                                 metadata_frame_duration);
    }
  }

  std::vector<base::TimeTicks> wall_clock_times;
  was_time_moving_ =
      wall_clock_time_cb_.Run(media_timestamps, &wall_clock_times);
  DCHECK_EQ(wall_clock_times.size(),
            frame_queue_.size() + (have_metadata_duration ? 1 : 0));

  // Transfer the converted wall clock times into our frame queue. Never process
  // the last frame in this loop; the last frame timing is handled below.
  for (size_t i = 0; i < frame_queue_.size() - 1; ++i) {
    ReadyFrame& frame = frame_queue_[i];

    // Whenever a frame is added to the queue, |has_estimated_end_time| is true;
    // this remains true until we receive a later frame -- from which we use its
    // timestamp to assign the true |end_time| for the previous frame.
    //
    // So a new sample can always be determined by the |has_estimated_end_time|
    // flag and the fact that the frame is being processed in this loop which
    // never processes the last (and thus always estimated) frame.
    const bool new_sample = frame.has_estimated_end_time;

    frame.start_time = wall_clock_times[i];
    frame.end_time = wall_clock_times[i + 1];
    frame.has_estimated_end_time = false;
    if (new_sample)
      frame_duration_calculator_.AddSample(frame.end_time - frame.start_time);
  }

  base::TimeDelta deviation;
  if (frame_duration_calculator_.Count()) {
    // Compute |average_frame_duration_|, a moving average of the last few
    // frames; see kMovingAverageSamples for the exact number.
    average_frame_duration_ = frame_duration_calculator_.Mean();
    deviation = frame_duration_calculator_.Deviation();
  }

  if (have_metadata_duration) {
    auto& frame = frame_queue_.back();
    frame.start_time = wall_clock_times.end()[-2];
    frame.end_time = wall_clock_times.end()[-1];

    // This path will be taken for frames after the very first, but we only want
    // to use our estimate of |average_frame_duration_| when we have no samples
    // in |frame_duration_calculator_| -- since it's a more accurate reflection
    // of the per-frame on screen time.
    if (!frame_duration_calculator_.Count()) {
      average_frame_duration_ = frame.end_time - frame.start_time;
      if (average_frame_duration_.is_zero())
        return;
    }
  } else {
    frame_queue_.back().start_time = wall_clock_times.back();

    // If |have_metadata_duration| is false and we don't have any subsequent
    // frames, we can't continue processing since the cadence estimate requires
    // |average_frame_duration_| and |deviation| to be non-zero.
    if (!frame_duration_calculator_.Count()) {
      return;
    }

    // Update the frame end time for the last frame based on the average.
    frame_queue_.back().end_time =
        frame_queue_.back().start_time + average_frame_duration_;
  }

  // ITU-R BR.265 recommends a maximum acceptable drift of +/- half of the frame
  // duration; there are other asymmetric, more lenient measures, that we're
  // forgoing in favor of simplicity.
  //
  // We'll always allow at least 16.66ms of drift since literature suggests it's
  // well below the floor of detection and is high enough to ensure stability
  // for 60fps content.
  max_acceptable_drift_ =
      std::max(average_frame_duration_ / 2, base::Seconds(1.0 / 60));

  // If we were called via RemoveExpiredFrames() and Render() was never called,
  // we may not have a render interval yet.
  if (render_interval_.is_zero())
    return;

  const bool cadence_changed = cadence_estimator_.UpdateCadenceEstimate(
      render_interval_, average_frame_duration_, deviation,
      max_acceptable_drift_);

  // No need to update cadence if there's been no change; cadence will be set
  // as frames are added to the queue.
  if (!cadence_changed)
    return;

  cadence_frame_counter_ = 0;
  UpdateCadenceForFrames();
}

void VideoRendererAlgorithm::UpdateCadenceForFrames() {
  for (size_t i = 0; i < frame_queue_.size(); ++i) {
    // It's always okay to adjust the ideal render count, since the cadence
    // selection method will still count its current render count towards
    // cadence selection.
    frame_queue_[i].ideal_render_count =
        cadence_estimator_.has_cadence()
            ? cadence_estimator_.GetCadenceForFrame(cadence_frame_counter_ + i)
            : 0;
  }
}

int VideoRendererAlgorithm::FindBestFrameByCadence() const {
  DCHECK(!frame_queue_.empty());
  if (!cadence_estimator_.has_cadence())
    return -1;

  DCHECK(!frame_queue_.empty());
  DCHECK(cadence_estimator_.has_cadence());
  const ReadyFrame& current_frame = frame_queue_.front();

  // If the current frame is below cadence, we should prefer it.
  if (current_frame.render_count < current_frame.ideal_render_count)
    return 0;

  // If the current frame is on cadence or over cadence, find the next frame
  // with a positive ideal render count.
  for (size_t i = 1; i < frame_queue_.size(); ++i) {
    if (frame_queue_[i].ideal_render_count > 0)
      return i;
  }

  // We don't have enough frames to find a better once by cadence.
  return -1;
}

int VideoRendererAlgorithm::FindBestFrameByCoverage(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    int* second_best) const {
  DCHECK(!frame_queue_.empty());

  // Find the frame which covers the most of the interval [deadline_min,
  // deadline_max]. Frames outside of the interval are considered to have no
  // coverage, while those which completely overlap the interval have complete
  // coverage.
  int best_frame_by_coverage = -1;
  base::TimeDelta best_coverage;
  std::vector<base::TimeDelta> coverage(frame_queue_.size(), base::TimeDelta());
  for (size_t i = 0; i < frame_queue_.size(); ++i) {
    const ReadyFrame& frame = frame_queue_[i];

    // Frames which start after the deadline interval have zero coverage.
    if (frame.start_time > deadline_max)
      break;

    // Clamp frame end times to a maximum of |deadline_max|.
    const base::TimeTicks end_time = std::min(deadline_max, frame.end_time);

    // Frames entirely before the deadline interval have zero coverage.
    if (end_time < deadline_min)
      continue;

    // If we're here, the current frame overlaps the deadline in some way; so
    // compute the duration of the interval which is covered.
    const base::TimeDelta duration =
        end_time - std::max(deadline_min, frame.start_time);

    coverage[i] = duration;
    if (coverage[i] > best_coverage) {
      best_frame_by_coverage = i;
      best_coverage = coverage[i];
    }
  }

  // Find the second best frame by coverage; done by zeroing the coverage for
  // the previous best and recomputing the maximum.
  *second_best = -1;
  if (best_frame_by_coverage >= 0) {
    coverage[best_frame_by_coverage] = base::TimeDelta();
    auto it = std::max_element(coverage.begin(), coverage.end());
    if (it->is_positive())
      *second_best = it - coverage.begin();
  }

  // If two frames have coverage within half a millisecond, prefer the earliest
  // frame as having the best coverage.  Value chosen via experimentation to
  // ensure proper coverage calculation for 24fps in 60Hz where +/- 100us of
  // jitter is present within the |render_interval_|. At 60Hz this works out to
  // an allowed jitter of 3%.
  const base::TimeDelta kAllowableJitter = base::Microseconds(500);
  if (*second_best >= 0 && best_frame_by_coverage > *second_best &&
      (best_coverage - coverage[*second_best]).magnitude() <=
          kAllowableJitter) {
    std::swap(best_frame_by_coverage, *second_best);
  }

  // TODO(dalecurtis): We may want to make a better decision about what to do
  // when multiple frames have equivalent coverage over an interval.  Jitter in
  // the render interval may result in irregular frame selection which may be
  // visible to a viewer.
  //
  // 23.974fps and 24fps in 60Hz are the most common susceptible rates, so
  // extensive tests have been added to ensure these cases work properly.

  return best_frame_by_coverage;
}

int VideoRendererAlgorithm::FindBestFrameByDrift(
    base::TimeTicks deadline_min,
    base::TimeDelta* selected_frame_drift) const {
  DCHECK(!frame_queue_.empty());

  int best_frame_by_drift = -1;
  *selected_frame_drift = base::TimeDelta::Max();

  for (size_t i = 0; i < frame_queue_.size(); ++i) {
    const base::TimeDelta drift =
        CalculateAbsoluteDriftForFrame(deadline_min, i);
    // We use <= here to prefer the latest frame with minimum drift.
    if (drift <= *selected_frame_drift) {
      *selected_frame_drift = drift;
      best_frame_by_drift = i;
    }
  }

  return best_frame_by_drift;
}

base::TimeDelta VideoRendererAlgorithm::CalculateAbsoluteDriftForFrame(
    base::TimeTicks deadline_min,
    int frame_index) const {
  const ReadyFrame& frame = frame_queue_[frame_index];
  // If the frame lies before the deadline, compute the delta against the end
  // of the frame's duration.
  if (frame.end_time < deadline_min)
    return deadline_min - frame.end_time;

  // If the frame lies after the deadline, compute the delta against the frame's
  // start time.
  if (frame.start_time > deadline_min)
    return frame.start_time - deadline_min;

  // Drift is zero for frames which overlap the deadline interval.
  DCHECK_GE(deadline_min, frame.start_time);
  DCHECK_GE(frame.end_time, deadline_min);
  return base::TimeDelta();
}

void VideoRendererAlgorithm::UpdateEffectiveFramesQueued() {
  if (frame_queue_.empty() || average_frame_duration_.is_zero() ||
      last_deadline_max_.is_null()) {
    effective_frames_queued_ = frame_queue_.size();
    return;
  }

  // Determine the lower bound of the number of effective queues first.
  // Normally, this is 0.
  size_t min_frames_queued = 0;

  // If frame dropping is disabled, the lower bound is the number of frames
  // that were not rendered yet.
  if (frame_dropping_disabled_) {
    min_frames_queued =
        base::ranges::count(frame_queue_, 0, &ReadyFrame::render_count);
  }

  // Next, see if can report more frames as queued.
  effective_frames_queued_ =
      std::max(min_frames_queued, CountEffectiveFramesQueued());
}

int VideoRendererAlgorithm::FindFirstGoodFrame() const {
  const auto minimum_start_time =
      cadence_estimator_.has_cadence()
          ? last_deadline_max_ - max_acceptable_drift_
          : last_deadline_max_;

  size_t start_index = 0;
  for (; start_index < frame_queue_.size(); ++start_index) {
    const ReadyFrame& frame = frame_queue_[start_index];
    if ((!cadence_estimator_.has_cadence() ||
         frame.render_count < frame.ideal_render_count) &&
        (frame.end_time.is_null() || frame.end_time > minimum_start_time)) {
      break;
    }
  }

  return start_index == frame_queue_.size() ? -1 : start_index;
}

size_t VideoRendererAlgorithm::CountEffectiveFramesQueued() const {
  const int start_index = FindFirstGoodFrame();
  if (start_index < 0)
    return 0;

  if (!cadence_estimator_.has_cadence())
    return frame_queue_.size() - start_index;

  // We should ignore zero cadence frames in our effective frame count.
  size_t renderable_frame_count = 0;
  for (size_t i = start_index; i < frame_queue_.size(); ++i) {
    if (frame_queue_[i].ideal_render_count)
      ++renderable_frame_count;
  }
  return renderable_frame_count;
}

}  // namespace media
