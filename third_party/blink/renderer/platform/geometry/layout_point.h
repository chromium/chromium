/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_POINT_H_

#include <iosfwd>

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

class PLATFORM_EXPORT LayoutPoint {
  DISALLOW_NEW();

 public:
  constexpr LayoutPoint() = default;
  constexpr LayoutPoint(LayoutUnit x, LayoutUnit y) : x_(x), y_(y) {}
  constexpr LayoutPoint(int x, int y) : x_(LayoutUnit(x)), y_(LayoutUnit(y)) {}
  constexpr explicit LayoutPoint(const gfx::PointF& point)
      : x_(point.x()), y_(point.y()) {}

  constexpr explicit operator gfx::PointF() const {
    return gfx::PointF(x_.ToFloat(), y_.ToFloat());
  }

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  LayoutPoint(double, double) = delete;

  constexpr LayoutUnit X() const { return x_; }
  constexpr LayoutUnit Y() const { return y_; }

  WTF::String ToString() const;

 private:
  LayoutUnit x_, y_;
};

ALWAYS_INLINE constexpr bool operator==(const LayoutPoint& a,
                                        const LayoutPoint& b) {
  return a.X() == b.X() && a.Y() == b.Y();
}

constexpr bool operator!=(const LayoutPoint& a, const LayoutPoint& b) {
  return !(a == b);
}

inline gfx::Point ToRoundedPoint(const LayoutPoint& point) {
  return gfx::Point(point.X().Round(), point.Y().Round());
}

inline gfx::Vector2d ToRoundedVector2d(const LayoutPoint& p) {
  return gfx::Vector2d(p.X().Round(), p.Y().Round());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const LayoutPoint&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_POINT_H_
