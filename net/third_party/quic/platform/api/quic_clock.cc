// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/platform/api/quic_clock.h"

#include <limits>

#include "net/third_party/quic/platform/api/quic_logging.h"

namespace quic {

QuicClock::QuicClock()
    : is_calibrated_(false), calibration_offset_(QuicTime::Delta::Zero()) {}

QuicClock::~QuicClock() {}

QuicTime::Delta QuicClock::ComputeCalibrationOffset() const {
  // In the ideal world, all we need to do is to return the difference of
  // WallNow() and Now(). In the real world, things like context switch may
  // happen between the calls to WallNow() and Now(), causing their difference
  // to be arbitrarily large, so we repeat the calculation many times and use
  // the one with the minimum difference as the true offset.
  int64_t min_offset_us = std::numeric_limits<int64_t>::max();

  for (int i = 0; i < 128; ++i) {
    int64_t now_in_us = (Now() - QuicTime::Zero()).ToMicroseconds();
    int64_t wallnow_in_us =
        static_cast<int64_t>(WallNow().ToUNIXMicroseconds());

    int64_t offset_us = wallnow_in_us - now_in_us;
    if (offset_us < min_offset_us) {
      min_offset_us = offset_us;
    }
  }

  return QuicTime::Delta::FromMicroseconds(min_offset_us);
}

void QuicClock::SetCalibrationOffset(QuicTime::Delta offset) {
  DCHECK(!is_calibrated_) << "A clock should only be calibrated once";
  calibration_offset_ = offset;
  is_calibrated_ = true;
}

QuicTime QuicClock::ConvertWallTimeToQuicTime(
    const QuicWallTime& walltime) const {
  if (is_calibrated_) {
    int64_t time_in_us = static_cast<int64_t>(walltime.ToUNIXMicroseconds()) -
                         calibration_offset_.ToMicroseconds();
    return QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(time_in_us);
  }

  //     ..........................
  //     |            |           |
  // unix epoch   |walltime|   WallNow()
  //     ..........................
  //            |     |           |
  //     clock epoch  |         Now()
  //               result
  //
  // result = Now() - (WallNow() - walltime)
  return Now() - QuicTime::Delta::FromMicroseconds(
                     WallNow()
                         .Subtract(QuicTime::Delta::FromMicroseconds(
                             walltime.ToUNIXMicroseconds()))
                         .ToUNIXMicroseconds());
}

}  // namespace quic
