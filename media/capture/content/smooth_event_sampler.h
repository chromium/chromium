// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_SMOOTH_EVENT_SAMPLER_H_
#define MEDIA_CAPTURE_CONTENT_SMOOTH_EVENT_SAMPLER_H_

#include "base/time/time.h"
#include "media/capture/capture_export.h"

namespace media {

// Filters a sequence of events to achieve a target frequency.
class CAPTURE_EXPORT SmoothEventSampler {
 public:
  explicit SmoothEventSampler(base::TimeDelta min_capture_period);

  SmoothEventSampler(const SmoothEventSampler&) = delete;
  SmoothEventSampler& operator=(const SmoothEventSampler&) = delete;

  // Get/Set minimum capture period. When setting a new value, the state of the
  // sampler is retained so that sampling will continue smoothly.
  base::TimeDelta min_capture_period() const { return min_capture_period_; }
  void SetMinCapturePeriod(base::TimeDelta p);

  // Add a new event to the event history, and consider whether it ought to be
  // sampled. The event is not recorded as a sample until RecordSample() is
  // called.
  void ConsiderPresentationEvent(base::TimeTicks event_time);

  // Returns true if the last event considered should be sampled.
  bool ShouldSample() const;

  // Operates on the last event added by ConsiderPresentationEvent(), marking
  // it as sampled. After this point we are current in the stream of events, as
  // we have sampled the most recent event.
  void RecordSample();

  // Returns true if ConsiderPresentationEvent() has been called since the last
  // call to RecordSample().
  bool HasUnrecordedEvent() const;

 private:
  base::TimeDelta min_capture_period_;
  base::TimeDelta token_bucket_capacity_;

  base::TimeTicks current_event_;
  base::TimeTicks last_sample_;
  base::TimeDelta token_bucket_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_SMOOTH_EVENT_SAMPLER_H_
