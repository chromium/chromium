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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_H_

#include <iosfwd>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

// This class is deprecated. Use PhysicalRect or LogicalRect instead.
class PLATFORM_EXPORT DeprecatedLayoutRect {
  DISALLOW_NEW();

 public:
  constexpr DeprecatedLayoutRect() = default;
  constexpr DeprecatedLayoutRect(const LayoutPoint& location,
                                 const DeprecatedLayoutSize& size)
      : location_(location), size_(size) {}
  constexpr DeprecatedLayoutRect(LayoutUnit x,
                                 LayoutUnit y,
                                 LayoutUnit width,
                                 LayoutUnit height)
      : location_(LayoutPoint(x, y)),
        size_(DeprecatedLayoutSize(width, height)) {}
  constexpr DeprecatedLayoutRect(int x, int y, int width, int height)
      : location_(LayoutPoint(x, y)),
        size_(DeprecatedLayoutSize(width, height)) {}
  constexpr explicit operator gfx::RectF() const {
    return gfx::RectF(X(), Y(), Width(), Height());
  }

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  DeprecatedLayoutRect(double, double, double, double) = delete;

  constexpr LayoutPoint Location() const { return location_; }

  ALWAYS_INLINE constexpr LayoutUnit X() const { return location_.X(); }
  ALWAYS_INLINE constexpr LayoutUnit Y() const { return location_.Y(); }
  ALWAYS_INLINE LayoutUnit MaxX() const { return X() + Width(); }
  ALWAYS_INLINE LayoutUnit MaxY() const { return Y() + Height(); }
  constexpr LayoutUnit Width() const { return size_.Width(); }
  constexpr LayoutUnit Height() const { return size_.Height(); }

  void MoveBy(const LayoutPoint& offset) {
    location_.Move(offset.X(), offset.Y());
  }

  String ToString() const;

 private:
  LayoutPoint location_;
  DeprecatedLayoutSize size_;
};

inline gfx::Rect ToPixelSnappedRect(const DeprecatedLayoutRect& rect) {
  return gfx::Rect(ToRoundedPoint(rect.Location()),
                   gfx::Size(SnapSizeToPixel(rect.Width(), rect.X()),
                             SnapSizeToPixel(rect.Height(), rect.Y())));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const DeprecatedLayoutRect&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_H_
