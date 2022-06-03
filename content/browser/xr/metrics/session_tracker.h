// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_METRICS_SESSION_TRACKER_H_
#define CONTENT_BROWSER_XR_METRICS_SESSION_TRACKER_H_

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

// SessionTracker tracks UKM data for sessions and sends the data upon request.
template <class T>
class SessionTracker {
 public:
  explicit SessionTracker(std::unique_ptr<T> entry)
      : ukm_entry_(std::move(entry)),
        start_time_(base::Time::Now()),
        stop_time_(base::Time::Now()) {}
  virtual ~SessionTracker() {}
  T* ukm_entry() { return ukm_entry_.get(); }
  void SetSessionEnd(base::Time stop_time) { stop_time_ = stop_time; }

  int GetRoundedDurationInSeconds() {
    if (start_time_ > stop_time_) {
      // Return negative one to indicate an invalid value was recorded.
      return -1;
    }

    base::TimeDelta duration = stop_time_ - start_time_;
    DVLOG(1) << __func__ << ": " << duration.InSeconds();

    if (duration.InHours() > 1) {
      return duration.InHours() * 3600;
    } else if (duration.InMinutes() > 10) {
      return (duration.InMinutes() / 10) * 10 * 60;
    } else if (duration.InSeconds() > 60) {
      return duration.InMinutes() * 60;
    } else {
      return duration.InSeconds();
    }
  }

  void RecordEntry() {
    DVLOG(1) << __func__;
    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    DCHECK(ukm_recorder);

    ukm_entry_->Record(ukm_recorder);
  }

  SessionTracker(const SessionTracker&) = delete;
  SessionTracker& operator=(const SessionTracker&) = delete;

 protected:
  std::unique_ptr<T> ukm_entry_;

  base::Time start_time_;
  base::Time stop_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_METRICS_SESSION_TRACKER_H_
