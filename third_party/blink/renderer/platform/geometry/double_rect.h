// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_DOUBLE_RECT_H_

#include "third_party/blink/renderer/platform/geometry/double_point.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class FloatRect;
class IntRect;
class LayoutRect;

class PLATFORM_EXPORT DoubleRect {
  STACK_ALLOCATED();

 public:
  constexpr DoubleRect() = default;
  constexpr DoubleRect(const DoublePoint& location, const DoubleSize& size)
      : location_(location), size_(size) {}
  constexpr DoubleRect(double x, double y, double width, double height)
      : location_(DoublePoint(x, y)), size_(DoubleSize(width, height)) {}
  DoubleRect(const IntRect&);
  DoubleRect(const FloatRect&);
  DoubleRect(const LayoutRect&);

  constexpr DoublePoint Location() const { return location_; }
  constexpr DoubleSize Size() const { return size_; }

  void SetLocation(const DoublePoint& location) { location_ = location; }
  void SetSize(const DoubleSize& size) { size_ = size; }

  constexpr double X() const { return location_.X(); }
  constexpr double Y() const { return location_.Y(); }
  constexpr double MaxX() const { return X() + Width(); }
  constexpr double MaxY() const { return Y() + Height(); }
  constexpr double Width() const { return size_.Width(); }
  constexpr double Height() const { return size_.Height(); }

  void SetX(double x) { location_.SetX(x); }
  void SetY(double y) { location_.SetY(y); }
  void SetWidth(double width) { size_.SetWidth(width); }
  void SetHeight(double height) { size_.SetHeight(height); }

  constexpr bool IsEmpty() const { return size_.IsEmpty(); }
  constexpr bool IsZero() const { return size_.IsZero(); }

  void Move(const DoubleSize& delta) { location_ += delta; }
  void Move(double dx, double dy) { location_.Move(dx, dy); }
  void MoveBy(const DoublePoint& delta) {
    location_.Move(delta.X(), delta.Y());
  }

  constexpr DoublePoint MinXMinYCorner() const {
    return location_;
  }  // typically topLeft
  constexpr DoublePoint MaxXMinYCorner() const {
    return DoublePoint(location_.X() + size_.Width(), location_.Y());
  }  // typically topRight
  constexpr DoublePoint MinXMaxYCorner() const {
    return DoublePoint(location_.X(), location_.Y() + size_.Height());
  }  // typically bottomLeft
  constexpr DoublePoint MaxXMaxYCorner() const {
    return DoublePoint(location_.X() + size_.Width(),
                       location_.Y() + size_.Height());
  }  // typically bottomRight

  void Scale(float s) { Scale(s, s); }
  void Scale(float sx, float sy);

  String ToString() const;

 private:
  DoublePoint location_;
  DoubleSize size_;
};

PLATFORM_EXPORT IntRect EnclosingIntRect(const DoubleRect&);

// Returns a valid IntRect contained within the given DoubleRect.
PLATFORM_EXPORT IntRect EnclosedIntRect(const DoubleRect&);

PLATFORM_EXPORT IntRect RoundedIntRect(const DoubleRect&);

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const DoubleRect&);

}  // namespace blink

#endif
