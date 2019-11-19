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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_OUTSETS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_OUTSETS_H_

#include "third_party/blink/renderer/platform/geometry/float_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/int_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Specifies LayoutUnit lengths to be used to expand a rectangle.
// For example, |top()| returns the distance the top edge should be moved
// upward.
//
// Negative lengths can be used to express insets.
class PLATFORM_EXPORT LayoutRectOutsets {
  DISALLOW_NEW();

 public:
  constexpr LayoutRectOutsets() = default;
  constexpr LayoutRectOutsets(LayoutUnit top,
                              LayoutUnit right,
                              LayoutUnit bottom,
                              LayoutUnit left)
      : top_(top), right_(right), bottom_(bottom), left_(left) {}
  LayoutRectOutsets(int top, int right, int bottom, int left)
      : top_(LayoutUnit(top)),
        right_(LayoutUnit(right)),
        bottom_(LayoutUnit(bottom)),
        left_(LayoutUnit(left)) {}

  LayoutRectOutsets(const IntRectOutsets& outsets)
      : top_(LayoutUnit(outsets.Top())),
        right_(LayoutUnit(outsets.Right())),
        bottom_(LayoutUnit(outsets.Bottom())),
        left_(LayoutUnit(outsets.Left())) {}

  LayoutRectOutsets(const FloatRectOutsets& outsets)
      : top_(LayoutUnit(outsets.Top())),
        right_(LayoutUnit(outsets.Right())),
        bottom_(LayoutUnit(outsets.Bottom())),
        left_(LayoutUnit(outsets.Left())) {}

  constexpr LayoutUnit Top() const { return top_; }
  constexpr LayoutUnit Right() const { return right_; }
  constexpr LayoutUnit Bottom() const { return bottom_; }
  constexpr LayoutUnit Left() const { return left_; }

  void SetTop(LayoutUnit value) { top_ = value; }
  void SetRight(LayoutUnit value) { right_ = value; }
  void SetBottom(LayoutUnit value) { bottom_ = value; }
  void SetLeft(LayoutUnit value) { left_ = value; }

  LayoutSize Size() const { return LayoutSize(left_ + right_, top_ + bottom_); }
  constexpr bool IsZero() const {
    return !top_ && !right_ && !bottom_ && !left_;
  }

  void ClampNegativeToZero();

  void Unite(const LayoutRectOutsets&);

  void FlipHorizontally() { std::swap(left_, right_); }

  constexpr bool operator==(const LayoutRectOutsets other) const {
    return Top() == other.Top() && Right() == other.Right() &&
           Bottom() == other.Bottom() && Left() == other.Left();
  }

  String ToString() const;

 private:
  LayoutUnit top_;
  LayoutUnit right_;
  LayoutUnit bottom_;
  LayoutUnit left_;
};

inline LayoutRectOutsets& operator+=(LayoutRectOutsets& a,
                                     const LayoutRectOutsets& b) {
  a.SetTop(a.Top() + b.Top());
  a.SetRight(a.Right() + b.Right());
  a.SetBottom(a.Bottom() + b.Bottom());
  a.SetLeft(a.Left() + b.Left());
  return a;
}

inline LayoutRectOutsets& operator+=(LayoutRectOutsets& a, LayoutUnit b) {
  a.SetTop(a.Top() + b);
  a.SetRight(a.Right() + b);
  a.SetBottom(a.Bottom() + b);
  a.SetLeft(a.Left() + b);
  return a;
}

inline LayoutRectOutsets operator+(const LayoutRectOutsets& a,
                                   const LayoutRectOutsets& b) {
  return LayoutRectOutsets(a.Top() + b.Top(), a.Right() + b.Right(),
                           a.Bottom() + b.Bottom(), a.Left() + b.Left());
}

inline LayoutRectOutsets operator-(const LayoutRectOutsets& a) {
  return LayoutRectOutsets(-a.Top(), -a.Right(), -a.Bottom(), -a.Left());
}

inline LayoutRectOutsets& operator-=(LayoutRectOutsets& a,
                                     const LayoutRectOutsets& b) {
  a += -b;
  return a;
}

inline LayoutRectOutsets EnclosingLayoutRectOutsets(
    const FloatRectOutsets& rect) {
  return LayoutRectOutsets(LayoutUnit::FromFloatCeil(rect.Top()),
                           LayoutUnit::FromFloatCeil(rect.Right()),
                           LayoutUnit::FromFloatCeil(rect.Bottom()),
                           LayoutUnit::FromFloatCeil(rect.Left()));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const LayoutRectOutsets&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_RECT_OUTSETS_H_
