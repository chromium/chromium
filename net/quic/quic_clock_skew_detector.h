// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CLOCK_SKEW_DETECTOR_H_
#define NET_QUIC_QUIC_CLOCK_SKEW_DETECTOR_H_

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT_PRIVATE QuicClockSkewDetector {
 public:
  QuicClockSkewDetector(base::TimeTicks ticks_time, base::Time wall_time);

  // Returns true if the delta between |wall_now| and |last_wall_time_| is
  // more than one second larger than the delta between |ticks_now| and
  // |last_ticks_time_|.  Updates |last_ticks_time_| and |last_wall_time_|.
  bool ClockSkewDetected(base::TimeTicks ticks_now, base::Time wall_now);

 private:
  // Clock skew detection members
  base::TimeTicks last_ticks_time_;
  base::Time last_wall_time_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CLOCK_SKEW_DETECTOR_H_
