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
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

enum AspectRatioFit { kAspectRatioFitShrink, kAspectRatioFitGrow };

class PLATFORM_EXPORT LayoutSize {
  DISALLOW_NEW();

 public:
  constexpr LayoutSize() = default;
  constexpr explicit LayoutSize(const IntSize& size)
      : width_(size.Width()), height_(size.Height()) {}
  constexpr LayoutSize(LayoutUnit width, LayoutUnit height)
      : width_(width), height_(height) {}
  constexpr LayoutSize(int width, int height)
      : width_(LayoutUnit(width)), height_(LayoutUnit(height)) {}
  constexpr LayoutSize(float width, float height)
      : width_(LayoutUnit(width)), height_(LayoutUnit(height)) {}

  constexpr explicit LayoutSize(const FloatSize& size)
      : width_(size.Width()), height_(size.Height()) {}
  constexpr explicit LayoutSize(const DoubleSize& size)
      : width_(size.Width()), height_(size.Height()) {}
  constexpr explicit LayoutSize(const gfx::Size& size)
      : width_(size.width()), height_(size.height()) {}

  constexpr explicit operator FloatSize() const {
    return FloatSize(width_.ToFloat(), height_.ToFloat());
  }
  constexpr explicit operator FloatPoint() const {
    return FloatPoint(width_.ToFloat(), height_.ToFloat());
  }

  constexpr LayoutUnit Width() const { return width_; }
  constexpr LayoutUnit Height() const { return height_; }

  void SetWidth(LayoutUnit width) { width_ = width; }
  void SetHeight(LayoutUnit height) { height_ = height; }

  constexpr bool IsEmpty() const {
    return width_.RawValue() <= 0 || height_.RawValue() <= 0;
  }
  constexpr bool IsZero() const { return !width_ && !height_; }

  float AspectRatio() const { return width_.ToFloat() / height_.ToFloat(); }

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

  LayoutSize ExpandedTo(const IntSize& other) const {
    return LayoutSize(
        width_ > other.Width() ? width_ : LayoutUnit(other.Width()),
        height_ > other.Height() ? height_ : LayoutUnit(other.Height()));
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

  LayoutSize FitToAspectRatio(const LayoutSize& aspect_ratio,
                              AspectRatioFit fit) const {
    const float height_float = Height().ToFloat();
    const float width_float = Width().ToFloat();
    float height_scale = height_float / aspect_ratio.Height().ToFloat();
    float width_scale = width_float / aspect_ratio.Width().ToFloat();
    if ((width_scale > height_scale) != (fit == kAspectRatioFitGrow)) {
      return LayoutSize(
          height_float * aspect_ratio.Width() / aspect_ratio.Height(),
          Height());
    }
    return LayoutSize(
        Width(), width_float * aspect_ratio.Height() / aspect_ratio.Width());
  }

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

inline LayoutSize& operator-=(LayoutSize& a, const IntSize& b) {
  a.SetWidth(a.Width() - b.Width());
  a.SetHeight(a.Height() - b.Height());
  return a;
}

inline LayoutSize operator+(const LayoutSize& a, const LayoutSize& b) {
  return LayoutSize(a.Width() + b.Width(), a.Height() + b.Height());
}

inline LayoutSize operator+(const LayoutSize& a, const IntSize& b) {
  return LayoutSize(a.Width() + b.Width(), a.Height() + b.Height());
}

inline LayoutSize operator-(const LayoutSize& a, const LayoutSize& b) {
  return LayoutSize(a.Width() - b.Width(), a.Height() - b.Height());
}

inline LayoutSize operator-(const LayoutSize& size) {
  return LayoutSize(-size.Width(), -size.Height());
}

inline LayoutSize operator*(const LayoutSize& a, const float scale) {
  return LayoutSize(a.Width() * scale, a.Height() * scale);
}

constexpr bool operator==(const LayoutSize& a, const LayoutSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

inline bool operator==(const LayoutSize& a, const IntSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

constexpr bool operator!=(const LayoutSize& a, const LayoutSize& b) {
  return !(a == b);
}

inline bool operator!=(const LayoutSize& a, const IntSize& b) {
  return a.Width() != b.Width() || a.Height() != b.Height();
}

constexpr FloatPoint operator+(const FloatPoint& a, const LayoutSize& b) {
  return FloatPoint(a.X() + b.Width(), a.Y() + b.Height());
}

inline IntSize FlooredIntSize(const LayoutSize& s) {
  return IntSize(s.Width().Floor(), s.Height().Floor());
}

inline IntSize RoundedIntSize(const LayoutSize& s) {
  return IntSize(s.Width().Round(), s.Height().Round());
}

inline LayoutSize RoundedLayoutSize(const FloatSize& s) {
  return LayoutSize(s);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const LayoutSize&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&,
                                            const LayoutSize&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_
