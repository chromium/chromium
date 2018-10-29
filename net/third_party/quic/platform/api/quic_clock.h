// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_CLOCK_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_CLOCK_H_

#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// Interface for retrieving the current time.
class QUIC_EXPORT_PRIVATE QuicClock {
 public:
  QuicClock();
  virtual ~QuicClock();

  QuicClock(const QuicClock&) = delete;
  QuicClock& operator=(const QuicClock&) = delete;

  // Compute the offset between this clock with the Unix Epoch clock.
  // Return the calibrated offset between WallNow() and Now(), in the form of
  // (wallnow_in_us - now_in_us).
  // The return value can be used by SetCalibrationOffset() to actually
  // calibrate the clock, or all instances of this clock type.
  QuicTime::Delta ComputeCalibrationOffset() const;

  // Calibrate this clock. A calibrated clock gurantees that the
  // ConvertWallTimeToQuicTime() function always return the same result for the
  // same walltime.
  // Should not be called more than once for each QuicClock.
  void SetCalibrationOffset(QuicTime::Delta offset);

  // Returns the approximate current time as a QuicTime object.
  virtual QuicTime ApproximateNow() const = 0;

  // Returns the current time as a QuicTime object.
  // Note: this use significant resources please use only if needed.
  virtual QuicTime Now() const = 0;

  // WallNow returns the current wall-time - a time that is consistent across
  // different clocks.
  virtual QuicWallTime WallNow() const = 0;

  // Converts |walltime| to a QuicTime relative to this clock's epoch.
  virtual QuicTime ConvertWallTimeToQuicTime(
      const QuicWallTime& walltime) const;

 protected:
  // Creates a new QuicTime using |time_us| as the internal value.
  QuicTime CreateTimeFromMicroseconds(uint64_t time_us) const {
    return QuicTime(time_us);
  }

 private:
  // True if |calibration_offset_| is valid.
  bool is_calibrated_;
  // If |is_calibrated_|, |calibration_offset_| is the (fixed) offset between
  // the Unix Epoch clock and this clock.
  // In other words, the offset between WallNow() and Now().
  QuicTime::Delta calibration_offset_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_CLOCK_H_
