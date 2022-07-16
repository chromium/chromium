/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
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
#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_BOX_H_

#include <cmath>
#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/box_f.h"

namespace blink {

class PLATFORM_EXPORT FloatBox {
  DISALLOW_NEW();

 public:
  constexpr FloatBox()
      : x_(0), y_(0), z_(0), width_(0), height_(0), depth_(0) {}

  constexpr FloatBox(float x,
                     float y,
                     float z,
                     float width,
                     float height,
                     float depth)
      : x_(x), y_(y), z_(z), width_(width), height_(height), depth_(depth) {}

  constexpr FloatBox(const FloatBox&) = default;
  constexpr FloatBox& operator=(const FloatBox&) = default;

  explicit constexpr FloatBox(const gfx::BoxF& b)
      : x_(b.x()),
        y_(b.y()),
        z_(b.z()),
        width_(b.width()),
        height_(b.height()),
        depth_(b.depth()) {}

  // This is deleted during blink geometry type to gfx migration.
  // Use ToBoxF() instead.
  operator gfx::BoxF() const = delete;

  void set_origin(const FloatPoint3D& origin) {
    x_ = origin.x();
    y_ = origin.y();
    z_ = origin.z();
  }

  void set_size(const FloatPoint3D& origin) {
    DCHECK_GE(origin.x(), 0);
    DCHECK_GE(origin.y(), 0);
    DCHECK_GE(origin.z(), 0);

    width_ = origin.x();
    height_ = origin.y();
    depth_ = origin.z();
  }

  void Move(const FloatPoint3D& location) {
    x_ += location.x();
    y_ += location.y();
    z_ += location.z();
  }

  void Flatten() {
    z_ = 0;
    depth_ = 0;
  }

  void ExpandTo(const FloatPoint3D& low, const FloatPoint3D& high);
  void ExpandTo(const FloatPoint3D& point) { ExpandTo(point, point); }

  void ExpandTo(const FloatBox& box) {
    ExpandTo(FloatPoint3D(box.x(), box.y(), box.z()),
             FloatPoint3D(box.right(), box.bottom(), box.front()));
  }

  void Union(const FloatBox& box) {
    if (box.IsEmpty())
      return;

    if (IsEmpty()) {
      *this = box;
      return;
    }

    ExpandTo(box);
  }

  constexpr bool IsEmpty() const {
    return (width_ <= 0 && height_ <= 0) || (width_ <= 0 && depth_ <= 0) ||
           (height_ <= 0 && depth_ <= 0);
  }

  constexpr float right() const { return x_ + width_; }
  constexpr float bottom() const { return y_ + height_; }
  constexpr float front() const { return z_ + depth_; }
  constexpr float x() const { return x_; }
  constexpr float y() const { return y_; }
  constexpr float z() const { return z_; }
  constexpr float width() const { return width_; }
  constexpr float height() const { return height_; }
  constexpr float depth() const { return depth_; }

  String ToString() const;

 private:
  float x_;
  float y_;
  float z_;
  float width_;
  float height_;
  float depth_;
};

constexpr bool operator==(const FloatBox& a, const FloatBox& b) {
  return a.x() == b.x() && a.y() == b.y() && a.z() == b.z() &&
         a.width() == b.width() && a.height() == b.height() &&
         a.depth() == b.depth();
}

constexpr bool operator!=(const FloatBox& a, const FloatBox& b) {
  return !(a == b);
}

constexpr gfx::BoxF ToGfxBoxF(const FloatBox& b) {
  return gfx::BoxF(b.x(), b.y(), b.z(), b.width(), b.height(), b.depth());
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatBox&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_BOX_H_
