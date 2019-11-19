// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIME_DELTA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIME_DELTA_H_

#include "third_party/blink/renderer/core/animation/buildflags.h"

#if BUILDFLAG(BLINK_ANIMATION_USE_TIME_DELTA)
#include "base/time/time.h"
#endif

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include <limits>
#include <ostream>

namespace blink {

// AnimationTimeDelta exists to ease the transition of Blink animations from
// double-based time values to base::TimeDelta-based time values (see
// http://crbug.com/737867.
//
// It represents a delta between two times, analogous to base::TimeDelta. It is
// provided in two modes, based on a compiler flag. The first is the traditional
// (and default) double mode, where time deltas are represented using seconds in
// double-precision. The second mode uses base::TimeDelta to represent time
// instead.

#if !BUILDFLAG(BLINK_ANIMATION_USE_TIME_DELTA)

// The double-based version of AnimationTimeDelta. Internally, time is stored as
// double-precision seconds.
//
// This class is modelled on the API from base::TimeDelta, with a lot of
// unnecessary methods stripped out.
class CORE_EXPORT AnimationTimeDelta {
  USING_FAST_MALLOC(AnimationTimeDelta);

 public:
  constexpr AnimationTimeDelta() : delta_(0) {}

  static AnimationTimeDelta FromSecondsD(double time_s) {
    DCHECK(!std::isnan(time_s));
    return AnimationTimeDelta(time_s);
  }
  static AnimationTimeDelta FromMillisecondsD(double time_ms) {
    DCHECK(!std::isnan(time_ms));
    return AnimationTimeDelta(time_ms / 1000);
  }

  static constexpr AnimationTimeDelta Max() {
    return AnimationTimeDelta(std::numeric_limits<double>::infinity());
  }

  double InSecondsF() const { return delta_; }
  double InMillisecondsF() const { return delta_ * 1000; }

  bool is_max() const {
    return delta_ == std::numeric_limits<double>::infinity();
  }
  bool is_zero() const { return delta_ == 0; }

  AnimationTimeDelta operator+(AnimationTimeDelta other) const {
    return AnimationTimeDelta(delta_ + other.delta_);
  }
  AnimationTimeDelta& operator+=(AnimationTimeDelta other) {
    return *this = (*this + other);
  }
  template <typename T>
  AnimationTimeDelta operator*(T a) const {
    return AnimationTimeDelta(delta_ * a);
  }
  template <typename V>
  AnimationTimeDelta& operator*=(V a) {
    return *this = (*this * a);
  }

 protected:
  constexpr explicit AnimationTimeDelta(double delta) : delta_(delta) {}

  // The time delta represented by this |AnimationTimeDelta|, in seconds. May be
  // negative, in which case the end of the delta is before the start.
  double delta_;
};

template <typename T>
AnimationTimeDelta operator*(T a, AnimationTimeDelta td) {
  return td * a;
}

// Comparison operators on AnimationTimeDelta.
bool CORE_EXPORT operator==(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs);
bool CORE_EXPORT operator!=(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs);
bool CORE_EXPORT operator>(const AnimationTimeDelta& lhs,
                           const AnimationTimeDelta& rhs);
bool CORE_EXPORT operator<(const AnimationTimeDelta& lhs,
                           const AnimationTimeDelta& rhs);
bool CORE_EXPORT operator>=(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs);
bool CORE_EXPORT operator<=(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs);

// Defined to allow DCHECK_EQ/etc to work with the class.
CORE_EXPORT std::ostream& operator<<(std::ostream& os,
                                     const AnimationTimeDelta& time);

#else  // !BUILDFLAG(BLINK_ANIMATION_USE_TIME_DELTA)

// When compiling in TimeDelta-based mode, AnimationTimeDelta is equivalent to
// base::TimeDelta.
using AnimationTimeDelta = base::TimeDelta;

#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIME_DELTA_H_
