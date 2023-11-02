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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class PLATFORM_EXPORT LayoutSize {
  DISALLOW_NEW();

 public:
  constexpr LayoutSize() = default;
  constexpr explicit LayoutSize(const gfx::Size& size)
      : width_(size.width()), height_(size.height()) {}
  constexpr LayoutSize(LayoutUnit width, LayoutUnit height)
      : width_(width), height_(height) {}
  constexpr LayoutSize(int width, int height)
      : width_(LayoutUnit(width)), height_(LayoutUnit(height)) {}

  constexpr explicit LayoutSize(const gfx::SizeF& size)
      : width_(size.width()), height_(size.height()) {}
  constexpr explicit LayoutSize(const gfx::Vector2dF& vector)
      : width_(vector.x()), height_(vector.y()) {}

  constexpr explicit operator gfx::SizeF() const {
    return gfx::SizeF(width_.ToFloat(), height_.ToFloat());
  }
  constexpr explicit operator gfx::Vector2dF() const {
    return gfx::Vector2dF(width_.ToFloat(), height_.ToFloat());
  }

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  LayoutSize(double, double) = delete;

  constexpr LayoutUnit Width() const { return width_; }
  constexpr LayoutUnit Height() const { return height_; }

  void SetWidth(LayoutUnit width) { width_ = width; }
  void SetHeight(LayoutUnit height) { height_ = height; }

  constexpr bool IsEmpty() const {
    return width_.RawValue() <= 0 || height_.RawValue() <= 0;
  }
  constexpr bool IsZero() const { return !width_ && !height_; }

  void Expand(LayoutUnit width, LayoutUnit height) {
    width_ += width;
    height_ += height;
  }

  void Expand(int width, int height) {
    Expand(LayoutUnit(width), LayoutUnit(height));
  }

  void Shrink(int width, int height) {
    Shrink(LayoutUnit(width), LayoutUnit(height));
  }

  void Shrink(LayoutUnit width, LayoutUnit height) {
    width_ -= width;
    height_ -= height;
  }

  void Scale(float scale) {
    width_ *= scale;
    height_ *= scale;
  }

  void Scale(float width_scale, float height_scale) {
    width_ *= width_scale;
    height_ *= height_scale;
  }

  LayoutSize ExpandedTo(const LayoutSize& other) const {
    return LayoutSize(width_ > other.width_ ? width_ : other.width_,
                      height_ > other.height_ ? height_ : other.height_);
  }

  LayoutSize ShrunkTo(const LayoutSize& other) const {
    return LayoutSize(width_ < other.width_ ? width_ : other.width_,
                      height_ < other.height_ ? height_ : other.height_);
  }

  void ClampNegativeToZero() { *this = ExpandedTo(LayoutSize()); }

  void ClampToMinimumSize(const LayoutSize& minimum_size) {
    if (width_ < minimum_size.Width())
      width_ = minimum_size.Width();
    if (height_ < minimum_size.Height())
      height_ = minimum_size.Height();
  }

  LayoutSize TransposedSize() const { return LayoutSize(height_, width_); }

  LayoutSize Fraction() const {
    return LayoutSize(width_.Fraction(), height_.Fraction());
  }

  String ToString() const;

 private:
  LayoutUnit width_, height_;
};

inline LayoutSize& operator+=(LayoutSize& a, const LayoutSize& b) {
  a.SetWidth(a.Width() + b.Width());
  a.SetHeight(a.Height() + b.Height());
  return a;
}

inline LayoutSize& operator-=(LayoutSize& a, const LayoutSize& b) {
  a.SetWidth(a.Width() - b.Width());
  a.SetHeight(a.Height() - b.Height());
  return a;
}

inline LayoutSize& operator-=(LayoutSize& a, const gfx::Size& b) {
  a.SetWidth(a.Width() - b.width());
  a.SetHeight(a.Height() - b.height());
  return a;
}

inline LayoutSize operator+(const LayoutSize& a, const LayoutSize& b) {
  return LayoutSize(a.Width() + b.Width(), a.Height() + b.Height());
}

inline LayoutSize operator+(const LayoutSize& a, const gfx::Size& b) {
  return LayoutSize(a.Width() + b.width(), a.Height() + b.height());
}

inline LayoutSize operator-(const LayoutSize& a, const LayoutSize& b) {
  return LayoutSize(a.Width() - b.Width(), a.Height() - b.Height());
}

inline LayoutSize operator-(const LayoutSize& size) {
  return LayoutSize(-size.Width(), -size.Height());
}

inline LayoutSize operator*(const LayoutSize& a, const float scale) {
  return LayoutSize(LayoutUnit(a.Width() * scale),
                    LayoutUnit(a.Height() * scale));
}

constexpr bool operator==(const LayoutSize& a, const LayoutSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

constexpr bool operator!=(const LayoutSize& a, const LayoutSize& b) {
  return !(a == b);
}

constexpr gfx::PointF operator+(const gfx::PointF& a, const LayoutSize& b) {
  return gfx::PointF(a.x() + b.Width(), a.y() + b.Height());
}

inline gfx::Size ToFlooredSize(const LayoutSize& s) {
  return gfx::Size(s.Width().Floor(), s.Height().Floor());
}

inline gfx::Size ToRoundedSize(const LayoutSize& s) {
  return gfx::Size(s.Width().Round(), s.Height().Round());
}

inline LayoutSize RoundedLayoutSize(const gfx::SizeF& s) {
  return LayoutSize(s);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const LayoutSize&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&,
                                            const LayoutSize&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_
