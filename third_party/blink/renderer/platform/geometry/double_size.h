// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_SIZE_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class LayoutSize;

class PLATFORM_EXPORT DoubleSize {
  DISALLOW_NEW();

 public:
  constexpr DoubleSize() : width_(0), height_(0) {}
  constexpr DoubleSize(double width, double height)
      : width_(width), height_(height) {}
  constexpr explicit DoubleSize(const IntSize& p)
      : width_(p.Width()), height_(p.Height()) {}
  constexpr DoubleSize(const FloatSize& s)
      : width_(s.Width()), height_(s.Height()) {}
  explicit DoubleSize(const LayoutSize&);

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

inline IntSize FlooredIntSize(const DoubleSize& p) {
  return IntSize(clampTo<int>(floor(p.Width())),
                 clampTo<int>(floor(p.Height())));
}

inline IntSize RoundedIntSize(const DoubleSize& p) {
  return IntSize(clampTo<int>(round(p.Width())),
                 clampTo<int>(round(p.Height())));
}

inline IntSize ExpandedIntSize(const DoubleSize& p) {
  return IntSize(clampTo<int>(ceil(p.Width())), clampTo<int>(ceil(p.Height())));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const DoubleSize&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_SIZE_H_
