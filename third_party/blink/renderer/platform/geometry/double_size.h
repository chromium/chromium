// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_SIZE_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class LayoutSize;

class PLATFORM_EXPORT DoubleSize {
  DISALLOW_NEW();

 public:
  constexpr DoubleSize() : width_(0), height_(0) {}
  constexpr DoubleSize(double width, double height)
      : width_(width), height_(height) {}
  constexpr explicit DoubleSize(const gfx::Size& p)
      : width_(p.width()), height_(p.height()) {}
  constexpr DoubleSize(const FloatSize& s)
      : width_(s.width()), height_(s.height()) {}
  explicit DoubleSize(const LayoutSize&);
  explicit DoubleSize(const gfx::Vector2dF& v)
      : width_(v.x()), height_(v.y()) {}

  constexpr double Width() const { return width_; }
  constexpr double Height() const { return height_; }

  void SetWidth(double width) { width_ = width; }
  void SetHeight(double height) { height_ = height; }

  constexpr bool IsEmpty() const { return width_ <= 0 || height_ <= 0; }

  constexpr bool IsZero() const {
    // Not using fabs as it is not a constexpr in LLVM libc++
    return -std::numeric_limits<double>::epsilon() < width_ &&
           width_ < std::numeric_limits<double>::epsilon() &&
           -std::numeric_limits<double>::epsilon() < height_ &&
           height_ < std::numeric_limits<double>::epsilon();
  }

  void Expand(float width, float height) {
    width_ += width;
    height_ += height;
  }

  void Scale(float width_scale, float height_scale) {
    width_ = width_ * width_scale;
    height_ = height_ * height_scale;
  }

  void Scale(float scale) { this->Scale(scale, scale); }

  String ToString() const;

 private:
  double width_, height_;
};

inline DoubleSize& operator+=(DoubleSize& a, const DoubleSize& b) {
  a.SetWidth(a.Width() + b.Width());
  a.SetHeight(a.Height() + b.Height());
  return a;
}

inline DoubleSize& operator-=(DoubleSize& a, const DoubleSize& b) {
  a.SetWidth(a.Width() - b.Width());
  a.SetHeight(a.Height() - b.Height());
  return a;
}

constexpr DoubleSize operator+(const DoubleSize& a, const DoubleSize& b) {
  return DoubleSize(a.Width() + b.Width(), a.Height() + b.Height());
}

constexpr DoubleSize operator-(const DoubleSize& a, const DoubleSize& b) {
  return DoubleSize(a.Width() - b.Width(), a.Height() - b.Height());
}

constexpr bool operator==(const DoubleSize& a, const DoubleSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

constexpr bool operator!=(const DoubleSize& a, const DoubleSize& b) {
  return a.Width() != b.Width() || a.Height() != b.Height();
}

inline gfx::Size ToFlooredSize(const DoubleSize& p) {
  return gfx::Size(ClampTo<int>(floor(p.Width())),
                   ClampTo<int>(floor(p.Height())));
}

inline gfx::Size ToRoundedSize(const DoubleSize& p) {
  return gfx::Size(ClampTo<int>(round(p.Width())),
                   ClampTo<int>(round(p.Height())));
}

inline gfx::Size ToCeiledSize(const DoubleSize& p) {
  return gfx::Size(ClampTo<int>(ceil(p.Width())),
                   ClampTo<int>(ceil(p.Height())));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const DoubleSize&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_SIZE_H_
