// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QuicTime represents one point in time, stored in microsecond resolution.
// QuicTime is monotonically increasing, even across system clock adjustments.
// The epoch (time 0) of QuicTime is unspecified.
//
// This implementation wraps a int64_t of usec since the epoch.  While
// the epoch is the Unix epoch, do not depend on this fact because other
// implementations, like Chrome's, do NOT have the same epoch.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_TIME_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_TIME_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>

#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"

// TODO(vasilvv): replace with with ABSL_MUST_USE_RESULT once we're using absl.
#if defined(__clang__)
#define QUIC_TIME_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define QUIC_TIME_WARN_UNUSED_RESULT
#endif /* defined(__clang__) */

namespace quic {

class QuicClock;

// A QuicTime is a purely relative time. QuicTime values from different clocks
// cannot be compared to each other. If you need an absolute time, see
// QuicWallTime, below.
class QUIC_EXPORT_PRIVATE QuicTime {
 public:
  // A QuicTime::Delta represents the signed difference between two points in
  // time, stored in microsecond resolution.
  class QUIC_EXPORT_PRIVATE Delta {
   public:
    // Create a object with an offset of 0.
    static constexpr Delta Zero() { return Delta(0); }

    // Create a object with infinite offset time.
    static constexpr Delta Infinite() { return Delta(kQuicInfiniteTimeUs); }

    // Converts a number of seconds to a time offset.
    static constexpr Delta FromSeconds(int64_t secs) {
      return Delta(secs * 1000 * 1000);
    }

    // Converts a number of milliseconds to a time offset.
    static constexpr Delta FromMilliseconds(int64_t ms) {
      return Delta(ms * 1000);
    }

    // Converts a number of microseconds to a time offset.
    static constexpr Delta FromMicroseconds(int64_t us) { return Delta(us); }

    // Converts the time offset to a rounded number of seconds.
    inline int64_t ToSeconds() const { return time_offset_ / 1000 / 1000; }

    // Converts the time offset to a rounded number of milliseconds.
    inline int64_t ToMilliseconds() const { return time_offset_ / 1000; }

    // Converts the time offset to a rounded number of microseconds.
    inline int64_t ToMicroseconds() const { return time_offset_; }

    inline bool IsZero() const { return time_offset_ == 0; }

    inline bool IsInfinite() const {
      return time_offset_ == kQuicInfiniteTimeUs;
    }

    QuicString ToDebugValue() const;

   private:
    friend inline bool operator==(QuicTime::Delta lhs, QuicTime::Delta rhs);
    friend inline bool operator<(QuicTime::Delta lhs, QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator<<(QuicTime::Delta lhs, size_t rhs);
    friend inline QuicTime::Delta operator>>(QuicTime::Delta lhs, size_t rhs);

    friend inline QuicTime::Delta operator+(QuicTime::Delta lhs,
                                            QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator-(QuicTime::Delta lhs,
                                            QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator*(QuicTime::Delta lhs, int rhs);
    friend inline QuicTime::Delta operator*(QuicTime::Delta lhs, double rhs);

    friend inline QuicTime operator+(QuicTime lhs, QuicTime::Delta rhs);
    friend inline QuicTime operator-(QuicTime lhs, QuicTime::Delta rhs);
    friend inline QuicTime::Delta operator-(QuicTime lhs, QuicTime rhs);

    static const int64_t kQuicInfiniteTimeUs =
        std::numeric_limits<int64_t>::max();

    explicit constexpr Delta(int64_t time_offset) : time_offset_(time_offset) {}

    int64_t time_offset_;
    friend class QuicTime;
  };

  // Creates a new QuicTime with an internal value of 0.  IsInitialized()
  // will return false for these times.
  static constexpr QuicTime Zero() { return QuicTime(0); }

  // Creates a new QuicTime with an infinite time.
  static constexpr QuicTime Infinite() {
    return QuicTime(Delta::kQuicInfiniteTimeUs);
  }

  // Produce the internal value to be used when logging.  This value
  // represents the number of microseconds since some epoch.  It may
  // be the UNIX epoch on some platforms.  On others, it may
  // be a CPU ticks based value.
  inline int64_t ToDebuggingValue() const { return time_; }

  inline bool IsInitialized() const { return 0 != time_; }

 private:
  friend class QuicClock;

  friend inline bool operator==(QuicTime lhs, QuicTime rhs);
  friend inline bool operator<(QuicTime lhs, QuicTime rhs);
  friend inline QuicTime operator+(QuicTime lhs, QuicTime::Delta rhs);
  friend inline QuicTime operator-(QuicTime lhs, QuicTime::Delta rhs);
  friend inline QuicTime::Delta operator-(QuicTime lhs, QuicTime rhs);

  explicit constexpr QuicTime(int64_t time) : time_(time) {}

  int64_t time_;
};

// A QuicWallTime represents an absolute time that is globally consistent. In
// practice, clock-skew means that comparing values from different machines
// requires some flexibility.
class QUIC_EXPORT_PRIVATE QuicWallTime {
 public:
  // FromUNIXSeconds constructs a QuicWallTime from a count of the seconds
  // since the UNIX epoch.
  static constexpr QuicWallTime FromUNIXSeconds(uint64_t seconds) {
    return QuicWallTime(seconds * 1000000);
  }

  static constexpr QuicWallTime FromUNIXMicroseconds(uint64_t microseconds) {
    return QuicWallTime(microseconds);
  }

  // Zero returns a QuicWallTime set to zero. IsZero will return true for this
  // value.
  static constexpr QuicWallTime Zero() { return QuicWallTime(0); }

  // Returns the number of seconds since the UNIX epoch.
  uint64_t ToUNIXSeconds() const;
  // Returns the number of microseconds since the UNIX epoch.
  uint64_t ToUNIXMicroseconds() const;

  bool IsAfter(QuicWallTime other) const;
  bool IsBefore(QuicWallTime other) const;

  // IsZero returns true if this object is the result of calling |Zero|.
  bool IsZero() const;

  // AbsoluteDifference returns the absolute value of the time difference
  // between |this| and |other|.
  QuicTime::Delta AbsoluteDifference(QuicWallTime other) const;

  // Add returns a new QuicWallTime that represents the time of |this| plus
  // |delta|.
  QUIC_TIME_WARN_UNUSED_RESULT QuicWallTime Add(QuicTime::Delta delta) const;

  // Subtract returns a new QuicWallTime that represents the time of |this|
  // minus |delta|.
  QUIC_TIME_WARN_UNUSED_RESULT QuicWallTime
  Subtract(QuicTime::Delta delta) const;

 private:
  explicit constexpr QuicWallTime(uint64_t microseconds)
      : microseconds_(microseconds) {}

  uint64_t microseconds_;
};

// Non-member relational operators for QuicTime::Delta.
inline bool operator==(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.time_offset_ == rhs.time_offset_;
}
inline bool operator!=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return lhs.time_offset_ < rhs.time_offset_;
}
inline bool operator>(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return !(lhs < rhs);
}
inline QuicTime::Delta operator>>(QuicTime::Delta lhs, size_t rhs) {
  return QuicTime::Delta(lhs.time_offset_ >> rhs);
}

// Non-member relational operators for QuicTime.
inline bool operator==(QuicTime lhs, QuicTime rhs) {
  return lhs.time_ == rhs.time_;
}
inline bool operator!=(QuicTime lhs, QuicTime rhs) {
  return !(lhs == rhs);
}
inline bool operator<(QuicTime lhs, QuicTime rhs) {
  return lhs.time_ < rhs.time_;
}
inline bool operator>(QuicTime lhs, QuicTime rhs) {
  return rhs < lhs;
}
inline bool operator<=(QuicTime lhs, QuicTime rhs) {
  return !(rhs < lhs);
}
inline bool operator>=(QuicTime lhs, QuicTime rhs) {
  return !(lhs < rhs);
}

// Non-member arithmetic operators for QuicTime::Delta.
inline QuicTime::Delta operator+(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return QuicTime::Delta(lhs.time_offset_ + rhs.time_offset_);
}
inline QuicTime::Delta operator-(QuicTime::Delta lhs, QuicTime::Delta rhs) {
  return QuicTime::Delta(lhs.time_offset_ - rhs.time_offset_);
}
inline QuicTime::Delta operator*(QuicTime::Delta lhs, int rhs) {
  return QuicTime::Delta(lhs.time_offset_ * rhs);
}
inline QuicTime::Delta operator*(QuicTime::Delta lhs, double rhs) {
  return QuicTime::Delta(
      static_cast<int64_t>(std::llround(lhs.time_offset_ * rhs)));
}
inline QuicTime::Delta operator*(int lhs, QuicTime::Delta rhs) {
  return rhs * lhs;
}
inline QuicTime::Delta operator*(double lhs, QuicTime::Delta rhs) {
  return rhs * lhs;
}

// Non-member arithmetic operators for QuicTime and QuicTime::Delta.
inline QuicTime operator+(QuicTime lhs, QuicTime::Delta rhs) {
  return QuicTime(lhs.time_ + rhs.time_offset_);
}
inline QuicTime operator-(QuicTime lhs, QuicTime::Delta rhs) {
  return QuicTime(lhs.time_ - rhs.time_offset_);
}
inline QuicTime::Delta operator-(QuicTime lhs, QuicTime rhs) {
  return QuicTime::Delta(lhs.time_ - rhs.time_);
}

// Override stream output operator for gtest.
inline std::ostream& operator<<(std::ostream& output,
                                const QuicTime::Delta delta) {
  output << delta.ToDebugValue();
  return output;
}
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_TIME_H_
