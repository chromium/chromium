// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/rtp_time.h"

#include <limits>
#include <sstream>

namespace media {
namespace cast {

namespace {

// Returns the base::TimeDelta nearest to the time represented by a tick count
// in the given timebase.
base::TimeDelta TicksToTimeDelta(int64_t ticks, int timebase) {
  DCHECK_GT(timebase, 0);
  const double micros = static_cast<double>(ticks) / timebase *
                            base::Time::kMicrosecondsPerSecond +
                                0.5 /* rounding */;
  DCHECK_LT(micros, static_cast<double>(std::numeric_limits<int64_t>::max()));
  return base::Microseconds(static_cast<int64_t>(micros));
}

// Returns the tick count in the given timebase nearest to the base::TimeDelta.
int64_t TimeDeltaToTicks(base::TimeDelta delta, int timebase) {
  DCHECK_GT(timebase, 0);
  const double ticks = delta.InSecondsF() * timebase + 0.5 /* rounding */;
  DCHECK_LT(ticks, static_cast<double>(std::numeric_limits<int64_t>::max()));
  return static_cast<int64_t>(ticks);
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const RtpTimeDelta rhs) {
  if (rhs.value_ >= 0)
    out << "RTP+";
  else
    out << "RTP";
  return out << rhs.value_;
}

std::ostream& operator<<(std::ostream& out, const RtpTimeTicks rhs) {
  return out << "RTP@" << rhs.value_;
}

base::TimeDelta RtpTimeDelta::ToTimeDelta(int rtp_timebase) const {
  return TicksToTimeDelta(value_, rtp_timebase);
}

// static
RtpTimeDelta RtpTimeDelta::FromTimeDelta(base::TimeDelta delta,
                                         int rtp_timebase) {
  return RtpTimeDelta(TimeDeltaToTicks(delta, rtp_timebase));
}

// static
RtpTimeDelta RtpTimeDelta::FromTicks(int64_t ticks) {
  return RtpTimeDelta(ticks);
}

base::TimeDelta RtpTimeTicks::ToTimeDelta(int rtp_timebase) const {
  return TicksToTimeDelta(value_, rtp_timebase);
}

// static
RtpTimeTicks RtpTimeTicks::FromTimeDelta(base::TimeDelta delta,
                                         int rtp_timebase) {
  return RtpTimeTicks(TimeDeltaToTicks(delta, rtp_timebase));
}

}  // namespace cast
}  // namespace media
