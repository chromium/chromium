// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_ALGORITHM_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_ALGORITHM_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/moving_window.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_renderer.h"
#include "media/filters/video_cadence_estimator.h"

namespace media {

class MediaLog;

// VideoRendererAlgorithm manages a queue of VideoFrames from which it chooses
// frames with the goal of providing a smooth playback experience.  I.e., the
// selection process results in the best possible uniformity for displayed frame
// durations over time.
//
// Clients will provide frames to VRA via EnqueueFrame() and then VRA will yield
// one of those frames in response to a future Render() call.  Each Render()
// call takes a render interval which is used to compute the best frame for
// display during that interval.
//
// Render() calls are expected to happen on a regular basis.  Failure to do so
// will result in suboptimal rendering experiences.  If a client knows that
// Render() callbacks are stalled for any reason, it should tell VRA to expire
// frames which are unusable via RemoveExpiredFrames(); this prevents useless
// accumulation of stale VideoFrame objects (which are frequently quite large).
//
// The primary means of smooth frame selection is via forced integer cadence,
// see VideoCadenceEstimator for details on this process.  In cases of non-
// integer cadence, the algorithm will fall back to choosing the frame which
// covers the most of the current render interval.  If no frame covers the
// current interval, the least bad frame will be chosen based on its drift from
// the start of the interval.
//
// Combined these three approaches enforce optimal smoothness in many cases.
class MEDIA_EXPORT VideoRendererAlgorithm {
 public:
  VideoRendererAlgorithm(const TimeSource::WallClockTimeCB& wall_clock_time_cb,
                         MediaLog* media_log);

  VideoRendererAlgorithm(const VideoRendererAlgorithm&) = delete;
  VideoRendererAlgorithm& operator=(const VideoRendererAlgorithm&) = delete;

  ~VideoRendererAlgorithm();

  // Chooses the best frame for the interval [deadline_min, deadline_max] based
  // on available and previously rendered frames.
  //
  // Under ideal circumstances the deadline interval provided to a Render() call
  // should be directly adjacent to the deadline given to the previous Render()
  // call with no overlap or gaps.  In practice, |deadline_max| is an estimated
  // value, which means the next |deadline_min| may overlap it slightly or have
  // a slight gap.  Gaps which exceed the length of the deadline interval are
  // assumed to be repeated frames for the purposes of cadence detection.
  //
  // If provided, |frames_dropped| will be set to the number of frames which
  // were removed from |frame_queue_|, during this call, which were never
  // returned during a previous Render() call and are no longer suitable for
  // rendering since their wall clock time is too far in the past.
  scoped_refptr<VideoFrame> Render(base::TimeTicks deadline_min,
                                   base::TimeTicks deadline_max,
                                   size_t* frames_dropped);

  // Removes all video frames which are unusable since their ideal render
  // interval [timestamp, timestamp + duration] is too far away from
  // |deadline_min| than is allowed by drift constraints.
  //
  // At least one frame will always remain after this call so that subsequent
  // Render() calls have a frame to return if no new frames are enqueued before
  // then.  Returns the number of frames removed that were never rendered.
  //
  // Note: In cases where there is no known frame duration (i.e. perhaps a video
  // with only a single frame), the last frame can not be expired, regardless of
  // the given deadline.  Clients must handle this case externally.
  size_t RemoveExpiredFrames(base::TimeTicks deadline);

  // Clients should call this if the last frame provided by Render() was never
  // rendered; it ensures the presented cadence matches internal models.  This
  // must be called before the next Render() call.
  void OnLastFrameDropped();

  // Adds a frame to |frame_queue_| for consideration by Render().  Out of order
  // timestamps will be sorted into appropriate order.  Do not enqueue end of
  // stream frames.  Frames inserted prior to the last rendered frame will not
  // be used.  They will be discarded on the next call to Render(), counting as
  // dropped frames, or by RemoveExpiredFrames(), counting as expired frames.
  //
  // Attempting to enqueue a frame with the same timestamp as a previous frame
  // will result in the previous frame being replaced if it has not been
  // rendered yet.  If it has been rendered, the new frame will be dropped.
  //
  // EnqueueFrame() will compute the current start time and an estimated end
  // time of the frame based on previous frames or the value of
  // VideoFrameMetadata::FRAME_DURATION if no previous frames, so that
  // EffectiveFramesQueued() is relatively accurate immediately after this call.
  void EnqueueFrame(scoped_refptr<VideoFrame> frame);

  // Removes all frames from the |frame_queue_| and clears predictors.  The
  // algorithm will be as if freshly constructed after this call.  By default
  // everything is reset, but if kPreserveNextFrameEstimates is specified, then
  // predictors for the start time of the next frame given to EnqueueFrame()
  // will be kept; allowing EffectiveFramesQueued() accuracy with one frame.
  enum class ResetFlag { kEverything, kPreserveNextFrameEstimates };
  void Reset(ResetFlag reset_flag = ResetFlag::kEverything);

  // Returns the number of frames currently buffered which could be rendered
  // assuming current Render() interval trends.
  //
  // If a cadence has been identified, this will return the number of frames
  // which have a non-zero ideal render count.
  //
  // If cadence has not been identified, this will return the number of frames
  // which have a frame end time greater than the end of the last render
  // interval passed to Render().  Note: If Render() callbacks become suspended
  // and the duration is unknown the last frame may never be stop counting as
  // effective.  Clients must handle this case externally.
  //
  // In either case, frames enqueued before the last displayed frame will not
  // be counted as effective.
  size_t effective_frames_queued() const { return effective_frames_queued_; }

  // Returns an estimate of the amount of memory (in bytes) used for frames.
  int64_t GetMemoryUsage() const;

  // Tells the algorithm that Render() callbacks have been suspended for a known
  // reason and such stoppage shouldn't be counted against future frames.
  void set_time_stopped() { was_time_moving_ = false; }

  size_t frames_queued() const { return frame_queue_.size(); }

  // Returns the average of the duration of all frames in |frame_queue_|
  // as measured in wall clock (not media) time.
  base::TimeDelta average_frame_duration() const {
    return average_frame_duration_;
  }

  // End time of the last frame.
  base::TimeTicks last_frame_end_time() const {
    return frame_queue_.back().end_time;
  }

  const VideoFrame& last_frame() const { return *frame_queue_.back().frame; }

  // Current render interval.
  base::TimeDelta render_interval() const { return render_interval_; }

  // Method used for testing which disables frame dropping, in this mode the
  // algorithm will never drop frames and instead always return every frame
  // for display at least once.
  void disable_frame_dropping() { frame_dropping_disabled_ = true; }

  enum : int {
    // The number of frames to store for moving average calculations.  Value
    // picked after experimenting with playback of various local media and
    // YouTube clips.
    kMovingAverageSamples = 32
  };

 private:
  friend class VideoRendererAlgorithmTest;

  // The determination of whether to clamp to a given cadence is based on the
  // number of seconds before a frame would have to be dropped or repeated to
  // compensate for reaching the maximum acceptable drift.
  //
  // We've chosen 8 seconds based on practical observations and the fact that it
  // allows 29.9fps and 59.94fps in 60Hz and vice versa.
  //
  // Most users will not be able to see a single frame repeated or dropped every
  // 8 seconds and certainly should notice it less than the randomly variable
  // frame durations.
  static const int kMinimumAcceptableTimeBetweenGlitchesSecs = 8;

  // Metadata container for enqueued frames.  See |frame_queue_| below.
  struct ReadyFrame {
    ReadyFrame(scoped_refptr<VideoFrame> frame);
    ReadyFrame(const ReadyFrame& other);
    ~ReadyFrame();

    // For use with std::lower_bound.
    bool operator<(const ReadyFrame& other) const;

    scoped_refptr<VideoFrame> frame;

    // |start_time| is only available after UpdateFrameStatistics() has been
    // called and |end_time| only after we have more than one frame.
    base::TimeTicks start_time;
    base::TimeTicks end_time;

    // True if this frame's end time is based on the average frame duration and
    // not the time of the next frame.
    bool has_estimated_end_time;

    int ideal_render_count;
    int render_count;
    int drop_count;
  };

  // Updates the render count for the last rendered frame based on the number
  // of missing intervals between Render() calls.
  void AccountForMissedIntervals(base::TimeTicks deadline_min,
                                 base::TimeTicks deadline_max);

  // Updates the render count and wall clock timestamps for all frames in
  // |frame_queue_|.  Updates |was_time_stopped_|, |cadence_estimator_| and
  // |frame_duration_calculator_|.
  //
  // Note: Wall clock time is recomputed each Render() call because it's
  // expected that the TimeSource powering TimeSource::WallClockTimeCB will skew
  // slightly based on the audio clock.
  //
  // TODO(dalecurtis): Investigate how accurate we need the wall clock times to
  // be, so we can avoid recomputing every time (we would need to recompute when
  // playback rate changes occur though).
  void UpdateFrameStatistics();

  // Updates the ideal render count for all frames in |frame_queue_| based on
  // the cadence returned by |cadence_estimator_|.  Cadence is assigned based
  // on |frame_counter_|.
  void UpdateCadenceForFrames();

  // If |cadence_estimator_| has detected a valid cadence, attempts to find the
  // next frame which should be rendered.  Returns -1 if not enough frames are
  // available for cadence selection or there is no cadence.
  int FindBestFrameByCadence() const;

  // Iterates over |frame_queue_| and finds the frame which covers the most of
  // the deadline interval.  If multiple frames have coverage of the interval,
  // |second_best| will be set to the index of the frame with the next highest
  // coverage.  Returns -1 if no frame has any coverage of the current interval.
  //
  // Prefers the earliest frame if multiple frames have similar coverage (within
  // a few percent of each other).
  int FindBestFrameByCoverage(base::TimeTicks deadline_min,
                              base::TimeTicks deadline_max,
                              int* second_best) const;

  // Iterates over |frame_queue_| and find the frame which drifts the least from
  // |deadline_min|.  There's always a best frame by drift, so the return value
  // is always a valid frame index.  |selected_frame_drift| will be set to the
  // drift of the chosen frame.
  //
  // Note: Drift calculations assume contiguous frames in the time domain, so
  // it's not possible to have a case where a frame is -10ms from |deadline_min|
  // and another frame which is at some time after |deadline_min|.  The second
  // frame would be considered to start at -10ms before |deadline_min| and would
  // overlap |deadline_min|, so its drift would be zero.
  int FindBestFrameByDrift(base::TimeTicks deadline_min,
                           base::TimeDelta* selected_frame_drift) const;

  // Calculates the drift from |deadline_min| for the given |frame_index|.  If
  // the [start_time, end_time] lies before |deadline_min| the drift is
  // the delta between |deadline_min| and |end_time|. If the frame
  // overlaps |deadline_min| the drift is zero. If the frame lies after
  // |deadline_min| the drift is the delta between |deadline_min| and
  // |start_time|.
  base::TimeDelta CalculateAbsoluteDriftForFrame(base::TimeTicks deadline_min,
                                                 int frame_index) const;

  // Returns the index of the first usable frame or -1 if no usable frames.
  int FindFirstGoodFrame() const;

  // Updates |effective_frames_queued_| which is typically called far more
  // frequently (~4x) than the value changes.  This must be called whenever
  // frames are added or removed from the queue or when any property of a
  // ReadyFrame within the queue changes.
  void UpdateEffectiveFramesQueued();

  // Computes the unclamped count of effective frames.  Used by
  // UpdateEffectiveFramesQueued().
  size_t CountEffectiveFramesQueued() const;

  raw_ptr<MediaLog> media_log_;
  int out_of_order_frame_logs_ = 0;

  // Queue of incoming frames waiting for rendering.
  using VideoFrameQueue = base::circular_deque<ReadyFrame>;
  VideoFrameQueue frame_queue_;

  // Handles cadence detection and frame cadence assignments.
  VideoCadenceEstimator cadence_estimator_;

  // Indicates if any calls to Render() have successfully yielded a frame yet.
  bool have_rendered_frames_;

  // Callback used to convert media timestamps into wall clock timestamps.
  const TimeSource::WallClockTimeCB wall_clock_time_cb_;

  // The last |deadline_max| provided to Render(), used to predict whether
  // frames were rendered over cadence between Render() calls.
  base::TimeTicks last_deadline_max_;

  // The average of the duration of all frames in |frame_queue_| as measured in
  // wall clock (not media) time at the time of the last Render().
  base::MovingAverageDeviation<base::TimeDelta> frame_duration_calculator_;
  base::TimeDelta average_frame_duration_;

  // The length of the last deadline interval given to Render(), updated at the
  // start of Render().
  base::TimeDelta render_interval_;

  // The maximum acceptable drift before a frame can no longer be considered for
  // rendering within a given interval.
  base::TimeDelta max_acceptable_drift_;

  // Indicates that the last call to Render() experienced a rendering glitch; it
  // may have: under-rendered a frame, over-rendered a frame, dropped one or
  // more frames, or chosen a frame which exceeded acceptable drift.
  bool last_render_had_glitch_;

  // For testing functionality which enables clockless playback of all frames,
  // does not prevent frame dropping due to equivalent timestamps.
  bool frame_dropping_disabled_;

  // Tracks frames dropped during enqueue when identical timestamps are added
  // to the queue.  Callers are told about these frames during Render().
  size_t frames_dropped_during_enqueue_;

  // When cadence is present, we don't want to start counting against cadence
  // until the first frame has reached its presentation time.
  bool first_frame_;

  // The frame number of the last rendered frame; incremented for every frame
  // rendered and every frame dropped or expired since the last rendered frame.
  //
  // Given to |cadence_estimator_| when assigning cadence values to the
  // ReadyFrameQueue.  Cleared when a new cadence is detected.
  uint64_t cadence_frame_counter_;

  // Indicates if time was moving, set to the return value from
  // UpdateFrameStatistics() during Render() or externally by
  // set_time_stopped().
  bool was_time_moving_;

  // Current number of effective frames in the |frame_queue_|.  Updated by calls
  // to UpdateEffectiveFramesQueued() whenever the |frame_queue_| is changed.
  size_t effective_frames_queued_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_ALGORITHM_H_
