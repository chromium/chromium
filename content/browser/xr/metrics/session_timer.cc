// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/metrics/session_timer.h"

#include "base/metrics/histogram_functions.h"

namespace content {

SessionTimer::SessionTimer(size_t trace_id) : trace_id_(trace_id) {}

SessionTimer::~SessionTimer() {
  StopSession();
}

size_t SessionTimer::GetTraceId() {
  return trace_id_;
}

base::Time SessionTimer::GetStartTime() {
  return start_time_;
}

void SessionTimer::StartSession() {
  DCHECK(start_time_.is_null())
      << "Must stop existing session before starting a new one";

  start_time_ = base::Time::Now();
}

void SessionTimer::StopSession() {
  if (start_time_.is_null()) {
    return;
  }
  // Calculate the duration of the session.
  base::TimeDelta session_duration = base::Time::Now() - start_time_;
  if (!session_duration.is_zero()) {
    // TODO(crbug.com/40677123): Consider renaming the timers to
    // something that indicates both that these also record AR, and that these
    // are no longer "suffixed" histograms.
    base::UmaHistogramCustomTimes("VRSessionTime.WebVR", session_duration,
                                  base::TimeDelta(), base::Hours(5), 100);
  }

  start_time_ = base::Time();
  trace_id_ = 0;
}

}  // namespace content
