// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_ANIMATED_CONTENT_SAMPLER_H_
#define MEDIA_CAPTURE_CONTENT_ANIMATED_CONTENT_SAMPLER_H_

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// Analyzes a sequence of events to detect the presence of constant frame rate
// animated content.  In the case where there are multiple regions of animated
// content, AnimatedContentSampler will propose sampling the one having the
// largest "smoothness" impact, according to human perception (e.g., a 24 FPS
// video versus a 60 FPS busy spinner).
//
// In addition, AnimatedContentSampler will provide rewritten frame timestamps,
// for downstream consumers, that are "truer" to the source content than to the
// local presentation hardware.
class CAPTURE_EXPORT AnimatedContentSampler {
 public:
  explicit AnimatedContentSampler(base::TimeDelta min_capture_period);
  ~AnimatedContentSampler();

  // Sets a new minimum capture period.
  void SetMinCapturePeriod(base::TimeDelta period);

  // Get/Set the target sampling period.  This is used to determine whether to
  // subsample the frames of animated content.
  base::TimeDelta target_sampling_period() const {
    return target_sampling_period_;
  }
  void SetTargetSamplingPeriod(base::TimeDelta period);

  // Examines the given presentation event metadata, along with recent history,
  // to detect animated content, updating the state of this sampler.
  // |damage_rect| is the region of a frame about to be drawn, while
  // |event_time| refers to the frame's estimated presentation time.
  void ConsiderPresentationEvent(const gfx::Rect& damage_rect,
                                 base::TimeTicks event_time);

  // Returns true if animated content has been detected and a decision has been
  // made about whether to sample the last event.
  bool HasProposal() const;

  // Returns true if the last event considered should be sampled.
  bool ShouldSample() const;

  // Returns a frame timestamp to provide to consumers of the sampled frame.
  // Only valid when ShouldSample() returns true.
  base::TimeTicks frame_timestamp() const { return frame_timestamp_; }

  // Returns the current sampling period.  This can be treated as the estimated
  // duration of the frame to be sampled.  Only valid when HasProposal()
  // returns true.
  base::TimeDelta sampling_period() const { return sampling_period_; }

  // Accessors to currently-detected animating region/period, for logging.
  const gfx::Rect& detected_region() const { return detected_region_; }
  base::TimeDelta detected_period() const { return detected_period_; }

  // Records that a frame with the given |frame_timestamp| was sampled.  This
  // method should be called when *any* sampling is taken, even if it was not
  // proposed by AnimatedContentSampler.
  void RecordSample(base::TimeTicks frame_timestamp);

 private:
  friend class AnimatedContentSamplerTest;

  // Data structure for efficient online analysis of recent event history.
  struct Observation {
    gfx::Rect damage_rect;
    base::TimeTicks event_time;

    Observation(const gfx::Rect& d, base::TimeTicks e)
        : damage_rect(d), event_time(e) {}
  };
  using ObservationFifo = base::circular_deque<Observation>;

  // Adds an observation to |observations_|, and prunes-out the old ones.
  void AddObservation(const gfx::Rect& damage_rect, base::TimeTicks event_time);

  // Returns the damage Rect that is responsible for the majority of the pixel
  // damage in recent event history, if there is such a Rect.  If there isn't,
  // this method could still return any Rect, so the caller must confirm the
  // returned Rect really is responsible for the majority of pixel damage.
  gfx::Rect ElectMajorityDamageRect() const;

  // Analyzes the observations relative to the current |event_time| to detect
  // stable animating content.  If detected, returns true and sets the output
  // arguments to the region of the animating content and its mean frame
  // duration.
  bool AnalyzeObservations(base::TimeTicks event_time,
                           gfx::Rect* rect,
                           base::TimeDelta* period) const;

  // Called by ConsiderPresentationEvent() when the current event is part of a
  // detected animation, to update |frame_timestamp_|.
  base::TimeTicks ComputeNextFrameTimestamp(base::TimeTicks event_time) const;

  // When the animation frame rate is greater than the target sampling rate,
  // this function determines an integer division of the animation frame rate
  // that is closest to the target sampling rate.  Returns the inverse of that
  // result (the period).  If the animation frame rate is slower or the same as
  // the target sampling rate, this function just returns |animation_period|.
  static base::TimeDelta ComputeSamplingPeriod(
      base::TimeDelta animation_period,
      base::TimeDelta target_sampling_period,
      base::TimeDelta min_capture_period);

  // The client expects frame timestamps to be at least this far apart.
  base::TimeDelta min_capture_period_;

  // A recent history of observations in chronological order, maintained by
  // AddObservation().
  ObservationFifo observations_;

  // The region of currently-detected animated content.  If empty, that means
  // "not detected."
  gfx::Rect detected_region_;

  // The mean frame duration of currently-detected animated content.  If zero,
  // that means "not detected."
  base::TimeDelta detected_period_;

  // Target period between sampled frames.  This can be changed by the client at
  // any time (e.g., to sample high frame rate content at a lower rate).
  base::TimeDelta target_sampling_period_;

  // The sampling period computed during the last call to
  // ConsiderPresentationEvent().
  base::TimeDelta sampling_period_;

  // Indicates whether the last event caused animated content to be detected and
  // whether the current event should be sampled.
  enum {
    NOT_SAMPLING,
    START_SAMPLING,
    SHOULD_NOT_SAMPLE,
    SHOULD_SAMPLE
  } sampling_state_;

  // A token bucket that is used to decide which subset of the frames containing
  // the animated content should be sampled.  Here, the smallest discrete unit
  // of time (one microsecond) equals one token; and, tokens are only taken from
  // the bucket when at least a full sampling period's worth are present.
  base::TimeDelta token_bucket_;

  // The rewritten frame timestamp for the latest event.
  base::TimeTicks frame_timestamp_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_ANIMATED_CONTENT_SAMPLER_H_
