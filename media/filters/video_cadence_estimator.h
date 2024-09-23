// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_CADENCE_ESTIMATOR_H_
#define MEDIA_FILTERS_VIDEO_CADENCE_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Estimates whether a given frame duration and render interval length have a
// render cadence which would allow for optimal uniformity of displayed frame
// durations over time.
//
// Cadence is the ideal repeating frame pattern for a group of frames; currently
// VideoCadenceEstimator supports N-frame ([a1:a2:..:aN]) cadences where N <= 5.
// Details on what this means are below.
//
// The perfect cadence of a set of frames is the ratio of the frame duration to
// render interval length.  I.e. for 30fps in 60Hz the cadence would be (1/30) /
// (1/60) = 60 / 30 = 2.  It's common that this is not an exact integer, e.g.,
// 29.974fps in 60Hz which would have a cadence of (1/29.974) / (1/60) =
// ~2.0029.
//
// The perfect cadence is always a real number.  All N-cadences [a1:a2:..:aN]
// where aK is an integer are an approximation of the perfect cadence; i.e. the
// average of [a1:..:aN] will approximately equal the perfect cadence.  When N=1
// we have a 1-frame cadence, when N=2, we have a 2-frame cadence, etc.
//
// For single frame cadence we just round the perfect cadence (~2.0029 in the
// previous example) to the nearest integer value (2 in this case; which is
// denoted as a cadence of [2]).  If the delta between those values is small we
// can choose to render frames for the integer number of render intervals;
// shortening or lengthening the actual rendered frame duration.  Doing so
// ensures each frame gets an optimal amount of display time.
//
// For N-frame cadence, the idea is similar, we just round the perfect cadence
// to some K/N, where K is an integer, and distribute [floor(K/N), floor(K/N)+1]
// into the cadence vector as evenly as possible.  For example, 23.97fps in
// 60Hz, the perfect cadence is 2.50313, we can round it to 2.5 = 5/2, and we
// can then construct the cadence vector as [2:3].
//
// The delta between the perfect cadence and the rounded cadence leads to drift
// over time of the actual VideoFrame timestamp relative to its rendered time,
// so we perform some calculations to ensure we only use a cadence when it will
// take some time to drift an undesirable amount; see CalculateCadence() for
// details on how this calculation is made.
//
// In practice this works out to the following for common setups if we use
// cadence based selection:
//
//    29.5fps in 60Hz,    ~17ms max drift => exhausted in ~1 second.
//    29.9fps in 60Hz,    ~17ms max drift => exhausted in ~16.4 seconds.
//    24fps   in 59.9Hz,  ~21ms max drift => exhausted in ~12.6 seconds.
//    24.9fps in 60Hz,    ~20ms max drift => exhausted in ~4.0 seconds.
//    59.9fps in 60Hz,    ~8.3ms max drift => exhausted in ~8.2 seconds.
//    24.9fps in 50Hz,    ~20ms max drift => exhausted in ~20.5 seconds.
//    120fps  in 59.9Hz,  ~8.3ms max drift => exhausted in ~8.2 seconds.
//
class MEDIA_EXPORT VideoCadenceEstimator {
 public:
  using Cadence = std::vector<int>;

  // As mentioned in the introduction, the determination of whether to clamp to
  // a given cadence is based on how long it takes before a frame would have to
  // be dropped or repeated to compensate for reaching the maximum acceptable
  // drift; this time length is controlled by |minimum_time_until_max_drift|.
  explicit VideoCadenceEstimator(base::TimeDelta minimum_time_until_max_drift);

  VideoCadenceEstimator(const VideoCadenceEstimator&) = delete;
  VideoCadenceEstimator& operator=(const VideoCadenceEstimator&) = delete;

  ~VideoCadenceEstimator();

  // Clears stored cadence information.
  void Reset();

  // Updates the estimates for |cadence_| based on the given values as described
  // in the introduction above.
  //
  // Clients should call this and then update the cadence for all frames via the
  // GetCadenceForFrame() method if the cadence changes.
  //
  // Cadence changes will not take affect until enough render intervals have
  // elapsed.  For the purposes of hysteresis, each UpdateCadenceEstimate() call
  // is assumed to elapse one |render_interval| worth of time.
  //
  // Returns true if the cadence has changed since the last call.
  bool UpdateCadenceEstimate(base::TimeDelta render_interval,
                             base::TimeDelta frame_duration,
                             base::TimeDelta frame_duration_deviation,
                             base::TimeDelta max_acceptable_drift);

  // Returns true if a useful cadence was found.
  bool has_cadence() const { return !cadence_.empty(); }

  // Given a |frame_number|, where zero is the most recently rendered frame,
  // returns the ideal cadence for that frame.
  //
  // Note: Callers must track the base |frame_number| relative to all frames
  // rendered or removed after the first frame for which cadence is detected.
  // The first frame after cadence is detected has a |frame_number| of 0.
  //
  // Frames which come in before the last rendered frame should be ignored in
  // terms of impact to the base |frame_number|.
  int GetCadenceForFrame(uint64_t frame_number) const;

  void set_cadence_hysteresis_threshold_for_testing(base::TimeDelta threshold) {
    cadence_hysteresis_threshold_ = threshold;
  }

  double avg_cadence_for_testing() const;
  size_t cadence_size_for_testing() const { return cadence_.size(); }
  std::string GetCadenceForTesting() const { return CadenceToString(cadence_); }

  // Determines whether a simple (single-valued) integer cadence exists for
  // |render_interval| and |frame_duration| that won't drift more than
  // |render_interval| within |minimum_time_until_max_drift|.
  static bool HasSimpleCadence(
      base::TimeDelta render_interval,
      base::TimeDelta frame_duration,
      base::TimeDelta minimum_time_until_max_drift = base::Seconds(8));

 private:
  // Attempts to find an N-frame cadence.  Returns the cadence vector if cadence
  // is found and sets |time_until_max_drift| for the computed cadence. If
  // multiple cadences satisfying the max drift constraint exist, we are going
  // to return the one with largest |time_until_max_drift|.
  // For details on the math and algorithm, see https://goo.gl/QK0vbz
  Cadence CalculateCadence(base::TimeDelta render_interval,
                           base::TimeDelta frame_duration,
                           base::TimeDelta max_acceptable_drift,
                           base::TimeDelta* time_until_max_drift) const;

  // Converts a cadence vector into a human readable string of the form
  // "[a: b: ...: z]".
  std::string CadenceToString(const Cadence& cadence) const;

  bool UpdateBresenhamCadenceEstimate(base::TimeDelta render_interval,
                                      base::TimeDelta frame_duration);

  void UpdateCadenceInternal(Cadence new_cadence,
                             base::TimeDelta time_until_max_drift);

  // The approximate best N-frame cadence for all frames seen thus far; updated
  // by UpdateCadenceEstimate().  Empty when no cadence has been detected.
  Cadence cadence_;

  // Used as hysteresis to prevent oscillation between cadence approximations
  // for spurious blips in the render interval or frame duration.
  //
  // Once a new cadence is detected, |render_intervals_cadence_held_| is
  // incremented for each UpdateCadenceEstimate() call where |cadence_| matches
  // |pending_cadence_|.  |render_intervals_cadence_held_| is cleared when a
  // "new" cadence matches |cadence_| or |pending_cadence_|.
  //
  // Once |kMinimumCadenceDurationMs| is exceeded in render intervals, the
  // detected cadence is set in |cadence_|.
  Cadence pending_cadence_;
  int render_intervals_cadence_held_;
  base::TimeDelta cadence_hysteresis_threshold_;

  // Tracks how many times cadence has switched during a given playback, used to
  // histogram the number of cadence changes in a playback.
  bool first_update_call_;
  int cadence_changes_;

  // The minimum amount of time allowed before a glitch occurs before confirming
  // cadence for a given render interval and frame duration.
  const base::TimeDelta minimum_time_until_max_drift_;

  bool is_variable_frame_rate_;
  base::TimeDelta last_render_interval_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_CADENCE_ESTIMATOR_H_
