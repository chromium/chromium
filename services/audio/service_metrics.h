// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_METRICS_H_
#define SERVICES_AUDIO_SERVICE_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}

namespace audio {

class ServiceMetrics {
 public:
  explicit ServiceMetrics(const base::TickClock* clock);

  ServiceMetrics(const ServiceMetrics&) = delete;
  ServiceMetrics& operator=(const ServiceMetrics&) = delete;

  ~ServiceMetrics();

  void HasConnections();
  void HasNoConnections();

 private:
  void LogHasNoConnectionsDuration();

  raw_ptr<const base::TickClock> clock_;
  base::TimeTicks has_connections_start_;
  base::TimeTicks has_no_connections_start_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_METRICS_H_
