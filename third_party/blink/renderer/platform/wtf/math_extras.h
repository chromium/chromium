/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_MATH_EXTRAS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_MATH_EXTRAS_H_

#include <cmath>
#include <cstddef>
#include <limits>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

#if defined(COMPILER_MSVC)
// Make math.h behave like other platforms.
#define _USE_MATH_DEFINES
// Even if math.h was already included, including math.h again with
// _USE_MATH_DEFINES adds the extra defines.
#include <math.h>
#include <stdint.h>
#endif

#if defined(OS_OPENBSD)
#include <machine/ieee.h>
#include <sys/types.h>
#endif

constexpr double kPiDouble = M_PI;
constexpr float kPiFloat = static_cast<float>(M_PI);

constexpr double kPiOverTwoDouble = M_PI_2;
constexpr float kPiOverTwoFloat = static_cast<float>(M_PI_2);

constexpr double kPiOverFourDouble = M_PI_4;
constexpr float kPiOverFourFloat = static_cast<float>(M_PI_4);

constexpr double kTwoPiDouble = kPiDouble * 2.0;
constexpr float kTwoPiFloat = kPiFloat * 2.0f;

constexpr double deg2rad(double d) {
  return d * kPiDouble / 180.0;
}
constexpr double rad2deg(double r) {
  return r * 180.0 / kPiDouble;
}
constexpr double deg2grad(double d) {
  return d * 400.0 / 360.0;
}
constexpr double grad2deg(double g) {
  return g * 360.0 / 400.0;
}
constexpr double turn2deg(double t) {
  return t * 360.0;
}
constexpr double deg2turn(double d) {
  return d / 360.0;
}
constexpr double rad2grad(double r) {
  return r * 200.0 / kPiDouble;
}
constexpr double grad2rad(double g) {
  return g * kPiDouble / 200.0;
}
constexpr double turn2grad(double t) {
  return t * 400;
}
constexpr double grad2turn(double g) {
  return g / 400;
}
constexpr double rad2turn(double r) {
  return r / kTwoPiDouble;
}
constexpr double turn2rad(double t) {
  return t * kTwoPiDouble;
}

constexpr float deg2rad(float d) {
  return d * kPiFloat / 180.0f;
}
constexpr float rad2deg(float r) {
  return r * 180.0f / kPiFloat;
}
constexpr float deg2grad(float d) {
  return d * 400.0f / 360.0f;
}
constexpr float grad2deg(float g) {
  return g * 360.0f / 400.0f;
}
constexpr float turn2deg(float t) {
  return t * 360.0f;
}
constexpr float deg2turn(float d) {
  return d / 360.0f;
}
constexpr float rad2grad(float r) {
  return r * 200.0f / kPiFloat;
}
constexpr float grad2rad(float g) {
  return g * kPiFloat / 200.0f;
}
constexpr float turn2grad(float t) {
  return t * 400;
}
constexpr float grad2turn(float g) {
  return g / 400;
}

// clampTo() is implemented by templated helper classes (to allow for partial
// template specialization) as well as several helper functions.

// This helper function can be called when we know that:
// (1) The type signednesses match so the compiler will not produce signed vs.
//     unsigned warnings
// (2) The default type promotions/conversions are sufficient to handle things
//     correctly
template <typename LimitType, typename ValueType>
inline LimitType clampToDirectComparison(ValueType value,
                                         LimitType min,
                                         LimitType max) {
  if (value >= max)
    return max;
  return (value <= min) ? min : static_cast<LimitType>(value);
}

// For any floating-point limits, or integral limits smaller than int64_t, we
// can cast the limits to double without losing precision; then the only cases
// where |value| can't be represented accurately as a double are the ones where
// it's outside the limit range anyway.  So doing all comparisons as doubles
// will give correct results.
//
// In some cases, we can get better performance by using
// clampToDirectComparison().  We use a templated class to switch between these
// two cases (instead of simply using a conditional within one function) in
// order to only compile the clampToDirectComparison() code for cases where it
// will actually be used; this prevents the compiler from emitting warnings
// about unsafe code (even though we wouldn't actually be executing that code).
template <bool canUseDirectComparison, typename LimitType, typename ValueType>
class ClampToNonLongLongHelper;
template <typename LimitType, typename ValueType>
class ClampToNonLongLongHelper<true, LimitType, ValueType> {
  STATIC_ONLY(ClampToNonLongLongHelper);

 public:
  static inline LimitType clampTo(ValueType value,
                                  LimitType min,
                                  LimitType max) {
    return clampToDirectComparison(value, min, max);
  }
};

template <typename LimitType, typename ValueType>
class ClampToNonLongLongHelper<false, LimitType, ValueType> {
  STATIC_ONLY(ClampToNonLongLongHelper);

 public:
  static inline LimitType clampTo(ValueType value,
                                  LimitType min,
                                  LimitType max) {
    const double doubleValue = static_cast<double>(value);
    if (doubleValue >= static_cast<double>(max))
      return max;
    if (doubleValue <= static_cast<double>(min))
      return min;
    // If the limit type is integer, we might get better performance by
    // casting |value| (as opposed to |doubleValue|) to the limit type.
    return std::numeric_limits<LimitType>::is_integer
               ? static_cast<LimitType>(value)
               : static_cast<LimitType>(doubleValue);
  }
};

// The unspecialized version of this templated class handles clamping to
// anything other than [u]int64_t limits.  It simply uses the class above
// to toggle between the "fast" and "safe" clamp implementations.
template <typename LimitType, typename ValueType>
class ClampToHelper {
 public:
  static inline LimitType clampTo(ValueType value,
                                  LimitType min,
                                  LimitType max) {
    // We only use clampToDirectComparison() when the integerness and
    // signedness of the two types matches.
    //
    // If the integerness of the types doesn't match, then at best
    // clampToDirectComparison() won't be much more efficient than the
    // cast-everything-to-double method, since we'll need to convert to
    // floating point anyway; at worst, we risk incorrect results when
    // clamping a float to a 32-bit integral type due to potential precision
    // loss.
    //
    // If the signedness doesn't match, clampToDirectComparison() will
    // produce warnings about comparing signed vs. unsigned, which are apt
    // since negative signed values will be converted to large unsigned ones
    // and we'll get incorrect results.
    return ClampToNonLongLongHelper <
                       std::numeric_limits<LimitType>::is_integer ==
                   std::numeric_limits<ValueType>::is_integer &&
               std::numeric_limits<LimitType>::is_signed ==
                   std::numeric_limits<ValueType>::is_signed,
           LimitType, ValueType > ::clampTo(value, min, max);
  }
};

// Clamping to [u]int64_t limits requires more care.  These may not be
// accurately representable as doubles, so instead we cast |value| to the
// limit type. But that cast is undefined if |value| is floating point and
// outside the representable range of the limit type, so we also have to check
// for that case explicitly.
template <typename ValueType>
class ClampToHelper<int64_t, ValueType> {
  STATIC_ONLY(ClampToHelper);

 public:
  static inline int64_t clampTo(ValueType value, int64_t min, int64_t max) {
    if (!std::numeric_limits<ValueType>::is_integer) {
      if (value > 0) {
        if (static_cast<double>(value) >=
            static_cast<double>(std::numeric_limits<int64_t>::max()))
          return max;
      } else if (static_cast<double>(value) <=
                 static_cast<double>(std::numeric_limits<int64_t>::min())) {
        return min;
      }
    }
    // Note: If |value| were uint64_t it could be larger than the largest
    // int64_t, and this code would be wrong; we handle  this case with
    // a separate full specialization below.
    return clampToDirectComparison(static_cast<int64_t>(value), min, max);
  }
};

// This specialization handles the case where the above partial specialization
// would be potentially incorrect.
template <>
class ClampToHelper<int64_t, uint64_t> {
  STATIC_ONLY(ClampToHelper);

 public:
  static inline int64_t clampTo(uint64_t value, int64_t min, int64_t max) {
    if (max <= 0 || value >= static_cast<uint64_t>(max))
      return max;
    const int64_t longLongValue = static_cast<int64_t>(value);
    return (longLongValue <= min) ? min : longLongValue;
  }
};

// This is similar to the partial specialization that clamps to int64_t, but
// because the lower-bound check is done for integer value types as well, we
// don't need a <uint64_t, int64_t> full specialization.
template <typename ValueType>
class ClampToHelper<uint64_t, ValueType> {
  STATIC_ONLY(ClampToHelper);

 public:
  static inline uint64_t clampTo(ValueType value, uint64_t min, uint64_t max) {
    if (value <= 0)
      return min;
    if (!std::numeric_limits<ValueType>::is_integer) {
      if (static_cast<double>(value) >=
          static_cast<double>(std::numeric_limits<uint64_t>::max()))
        return max;
    }
    return clampToDirectComparison(static_cast<uint64_t>(value), min, max);
  }
};

template <typename T>
constexpr T defaultMaximumForClamp() {
  return std::numeric_limits<T>::max();
}
// This basically reimplements C++11's std::numeric_limits<T>::lowest().
template <typename T>
constexpr T defaultMinimumForClamp() {
  return std::numeric_limits<T>::min();
}
template <>
constexpr float defaultMinimumForClamp<float>() {
  return -std::numeric_limits<float>::max();
}
template <>
constexpr double defaultMinimumForClamp<double>() {
  return -std::numeric_limits<double>::max();
}

// And, finally, the actual function for people to call.
template <typename LimitType, typename ValueType>
inline LimitType clampTo(ValueType value,
                         LimitType min = defaultMinimumForClamp<LimitType>(),
                         LimitType max = defaultMaximumForClamp<LimitType>()) {
  DCHECK(!std::isnan(static_cast<double>(value)));
  DCHECK_LE(min, max);  // This also ensures |min| and |max| aren't NaN.
  return ClampToHelper<LimitType, ValueType>::clampTo(value, min, max);
}

constexpr bool isWithinIntRange(float x) {
  return x > static_cast<float>(std::numeric_limits<int>::min()) &&
         x < static_cast<float>(std::numeric_limits<int>::max());
}

static constexpr size_t greatestCommonDivisor(size_t a, size_t b) {
  return b ? greatestCommonDivisor(b, a % b) : a;
}

constexpr size_t lowestCommonMultiple(size_t a, size_t b) {
  return a && b ? a / greatestCommonDivisor(a, b) * b : 0;
}

#endif  // #ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_MATH_EXTRAS_H_
