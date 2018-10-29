// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_BLEND_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_BLEND_H_

#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

#include <type_traits>

namespace blink {

inline int Blend(int from, int to, double progress) {
  return static_cast<int>(lround(from + (to - from) * progress));
}

// For unsigned types.
template <typename T>
inline T Blend(T from, T to, double progress) {
  static_assert(std::is_integral<T>::value,
                "blend can only be used with integer types");
  return clampTo<T>(round(to > from ? from + (to - from) * progress
                                    : from - (from - to) * progress));
}

inline double Blend(double from, double to, double progress) {
  return from + (to - from) * progress;
}

inline float Blend(float from, float to, double progress) {
  return static_cast<float>(from + (to - from) * progress);
}

inline LayoutUnit Blend(LayoutUnit from, LayoutUnit to, double progress) {
  return LayoutUnit(from + (to - from) * progress);
}

inline IntPoint Blend(const IntPoint& from,
                      const IntPoint& to,
                      double progress) {
  return IntPoint(Blend(from.X(), to.X(), progress),
                  Blend(from.Y(), to.Y(), progress));
}

inline FloatPoint Blend(const FloatPoint& from,
                        const FloatPoint& to,
                        double progress) {
  return FloatPoint(Blend(from.X(), to.X(), progress),
                    Blend(from.Y(), to.Y(), progress));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_BLEND_H_
