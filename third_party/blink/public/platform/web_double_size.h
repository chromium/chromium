// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOUBLE_SIZE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOUBLE_SIZE_H_

#include "third_party/blink/public/platform/web_common.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/geometry/double_size.h"  // nogncheck
#else
#include <ui/gfx/geometry/size_f.h>
#include <ui/gfx/geometry/vector2d_f.h>
#endif

namespace blink {

class WebDoubleSize {
 public:
  bool IsEmpty() const { return width_ <= 0 || height_ <= 0; }

  WebDoubleSize() : width_(0), height_(0) {}

  WebDoubleSize(double width, double height) : width_(width), height_(height) {}

#if INSIDE_BLINK
  WebDoubleSize(const DoubleSize& size)
      : width_(size.Width()), height_(size.Height()) {}

  WebDoubleSize& operator=(const DoubleSize& size) {
    width_ = size.Width();
    height_ = size.Height();
    return *this;
  }

  operator DoubleSize() const { return DoubleSize(width_, height_); }
#else
  WebDoubleSize(const gfx::SizeF& size)
      : width_(size.width()), height_(size.height()) {}

  WebDoubleSize(const gfx::Vector2dF& vector)
      : width_(vector.x()), height_(vector.y()) {}

  WebDoubleSize& operator=(const gfx::SizeF& size) {
    width_ = size.width();
    height_ = size.height();
    return *this;
  }

  WebDoubleSize& operator=(const gfx::Vector2dF& vector) {
    width_ = vector.x();
    height_ = vector.y();
    return *this;
  }
#endif

  double Width() const { return width_; }
  double Height() const { return height_; }

 private:
  double width_;
  double height_;
};

inline bool operator==(const WebDoubleSize& a, const WebDoubleSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

inline bool operator!=(const WebDoubleSize& a, const WebDoubleSize& b) {
  return !(a == b);
}

}  // namespace blink

#endif
