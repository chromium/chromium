/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class SMILTime {
  DISALLOW_NEW();

 public:
  SMILTime() : time_(0) {}
  SMILTime(double time) : time_(time) {}

  static SMILTime Unresolved() {
    return std::numeric_limits<double>::quiet_NaN();
  }
  static SMILTime Indefinite() {
    return std::numeric_limits<double>::infinity();
  }

  double Value() const { return time_; }

  bool IsFinite() const { return std::isfinite(time_); }
  bool IsIndefinite() const { return std::isinf(time_); }
  bool IsUnresolved() const { return std::isnan(time_); }

 private:
  double time_;
};

class SMILTimeWithOrigin {
  DISALLOW_NEW();

 public:
  enum Origin { kParserOrigin, kScriptOrigin };

  SMILTimeWithOrigin() : origin_(kParserOrigin) {}

  SMILTimeWithOrigin(const SMILTime& time, Origin origin)
      : time_(time), origin_(origin) {}

  const SMILTime& Time() const { return time_; }
  bool OriginIsScript() const { return origin_ == kScriptOrigin; }

 private:
  SMILTime time_;
  Origin origin_;
};

struct SMILInterval {
  DISALLOW_NEW();
  SMILInterval() = default;
  SMILInterval(const SMILTime& begin, const SMILTime& end)
      : begin(begin), end(end) {}

  SMILTime begin;
  SMILTime end;
};

inline bool operator==(const SMILTime& a, const SMILTime& b) {
  return (a.IsUnresolved() && b.IsUnresolved()) || a.Value() == b.Value();
}
inline bool operator!(const SMILTime& a) {
  return !a.IsFinite() || !a.Value();
}
inline bool operator!=(const SMILTime& a, const SMILTime& b) {
  return !operator==(a, b);
}

// Ordering of SMILTimes has to follow: finite < indefinite (Inf) < unresolved
// (NaN). The first comparison is handled by IEEE754 but NaN values must be
// checked explicitly to guarantee that unresolved is ordered correctly too.
inline bool operator>(const SMILTime& a, const SMILTime& b) {
  return a.IsUnresolved() || (a.Value() > b.Value());
}
inline bool operator<(const SMILTime& a, const SMILTime& b) {
  return operator>(b, a);
}
inline bool operator>=(const SMILTime& a, const SMILTime& b) {
  return operator>(a, b) || operator==(a, b);
}
inline bool operator<=(const SMILTime& a, const SMILTime& b) {
  return operator<(a, b) || operator==(a, b);
}
inline bool operator<(const SMILTimeWithOrigin& a,
                      const SMILTimeWithOrigin& b) {
  return a.Time() < b.Time();
}

inline SMILTime operator+(const SMILTime& a, const SMILTime& b) {
  return a.Value() + b.Value();
}
inline SMILTime operator-(const SMILTime& a, const SMILTime& b) {
  return a.Value() - b.Value();
}
// So multiplying times does not make too much sense but SMIL defines it for
// duration * repeatCount
SMILTime operator*(const SMILTime&, const SMILTime&);

inline bool operator!=(const SMILInterval& a, const SMILInterval& b) {
  // Compare the "raw" values since the operator!= for SMILTime always return
  // true for non-finite times.
  return a.begin.Value() != b.begin.Value() || a.end.Value() != b.end.Value();
}

struct SMILTimeHash {
  STATIC_ONLY(SMILTimeHash);
  static unsigned GetHash(const SMILTime& key) {
    return WTF::FloatHash<double>::GetHash(key.Value());
  }
  static bool Equal(const SMILTime& a, const SMILTime& b) {
    return WTF::FloatHash<double>::Equal(a.Value(), b.Value());
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::SMILTime> {
  typedef blink::SMILTimeHash Hash;
};

template <>
struct HashTraits<blink::SMILTime> : GenericHashTraits<blink::SMILTime> {
  static blink::SMILTime EmptyValue() { return blink::SMILTime::Unresolved(); }
  static void ConstructDeletedValue(blink::SMILTime& slot, bool) {
    slot = -std::numeric_limits<double>::infinity();
  }
  static bool IsDeletedValue(blink::SMILTime value) {
    return value == -std::numeric_limits<double>::infinity();
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_H_
