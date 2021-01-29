// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SERVICE_METRICS_H_
#define SERVICES_AUDIO_SERVICE_METRICS_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}

namespace audio {

class ServiceMetrics {
 public:
  explicit ServiceMetrics(const base::TickClock* clock);
  ~ServiceMetrics();

  void HasConnections();
  void HasNoConnections();

 private:
  void LogHasNoConnectionsDuration();

  const base::TickClock* clock_;
  base::TimeTicks has_connections_start_;
  base::TimeTicks has_no_connections_start_;

  DISALLOW_COPY_AND_ASSIGN(ServiceMetrics);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SERVICE_METRICS_H_
