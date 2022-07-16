// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"

namespace audio {

ServiceMetrics::ServiceMetrics(const base::TickClock* clock) : clock_(clock) {}

ServiceMetrics::~ServiceMetrics() {
  LogHasNoConnectionsDuration();
}

void ServiceMetrics::HasConnections() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "Audio service has connections",
                                    TRACE_ID_LOCAL(this));
  has_connections_start_ = clock_->NowTicks();
  LogHasNoConnectionsDuration();
}

void ServiceMetrics::HasNoConnections() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "Audio service has connections",
                                  TRACE_ID_LOCAL(this));
  has_no_connections_start_ = clock_->NowTicks();
  DCHECK_NE(base::TimeTicks(), has_connections_start_);
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.HasConnectionsDuration",
                             clock_->NowTicks() - has_connections_start_,
                             base::TimeDelta(), base::Days(7), 50);
  has_connections_start_ = base::TimeTicks();
}

void ServiceMetrics::LogHasNoConnectionsDuration() {
  // Service shuts down without having accepted any connections in its lifetime
  // or with active connections, meaning there is no "no connection" interval in
  // progress.
  if (has_no_connections_start_.is_null())
    return;

  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.HasNoConnectionsDuration",
                             clock_->NowTicks() - has_no_connections_start_,
                             base::TimeDelta(), base::Minutes(10), 50);
  has_no_connections_start_ = base::TimeTicks();
}

}  // namespace audio
