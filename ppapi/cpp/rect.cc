// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/rect.h"

#include <algorithm>

namespace {

template<typename T>
void AdjustAlongAxis(T dst_origin, T dst_size,
                     T* origin, T* size) {
  if (*origin < dst_origin) {
    *origin = dst_origin;
    *size = std::min(dst_size, *size);
  } else {
    *size = std::min(dst_size, *size);
    *origin = std::min(dst_origin + dst_size, *origin + *size) - *size;
  }
}

}  // namespace

namespace pp {

void Rect::Inset(int32_t left, int32_t top, int32_t right, int32_t bottom) {
  Offset(left, top);
  set_width(std::max<int32_t>(width() - left - right, 0));
  set_height(std::max<int32_t>(height() - top - bottom, 0));
}

void Rect::Offset(int32_t horizontal, int32_t vertical) {
  rect_.point.x += horizontal;
  rect_.point.y += vertical;
}

bool Rect::Contains(int32_t point_x, int32_t point_y) const {
  return (point_x >= x()) && (point_x < right()) &&
         (point_y >= y()) && (point_y < bottom());
}

bool Rect::Contains(const Rect& rect) const {
  return (rect.x() >= x() && rect.right() <= right() &&
          rect.y() >= y() && rect.bottom() <= bottom());
}

bool Rect::Intersects(const Rect& rect) const {
  return !(rect.x() >= right() || rect.right() <= x() ||
           rect.y() >= bottom() || rect.bottom() <= y());
}

Rect Rect::Intersect(const Rect& rect) const {
  int32_t rx = std::max(x(), rect.x());
  int32_t ry = std::max(y(), rect.y());
  int32_t rr = std::min(right(), rect.right());
  int32_t rb = std::min(bottom(), rect.bottom());

  if (rx >= rr || ry >= rb)
    rx = ry = rr = rb = 0;  // non-intersecting

  return Rect(rx, ry, rr - rx, rb - ry);
}

Rect Rect::Union(const Rect& rect) const {
  // special case empty rects...
  if (IsEmpty())
    return rect;
  if (rect.IsEmpty())
    return *this;

  int32_t rx = std::min(x(), rect.x());
  int32_t ry = std::min(y(), rect.y());
  int32_t rr = std::max(right(), rect.right());
  int32_t rb = std::max(bottom(), rect.bottom());

  return Rect(rx, ry, rr - rx, rb - ry);
}

Rect Rect::Subtract(const Rect& rect) const {
  // boundary cases:
  if (!Intersects(rect))
    return *this;
  if (rect.Contains(*this))
    return Rect();

  int32_t rx = x();
  int32_t ry = y();
  int32_t rr = right();
  int32_t rb = bottom();

  if (rect.y() <= y() && rect.bottom() >= bottom()) {
    // complete intersection in the y-direction
    if (rect.x() <= x()) {
      rx = rect.right();
    } else {
      rr = rect.x();
    }
  } else if (rect.x() <= x() && rect.right() >= right()) {
    // complete intersection in the x-direction
    if (rect.y() <= y()) {
      ry = rect.bottom();
    } else {
      rb = rect.y();
    }
  }
  return Rect(rx, ry, rr - rx, rb - ry);
}

Rect Rect::AdjustToFit(const Rect& rect) const {
  int32_t new_x = x();
  int32_t new_y = y();
  int32_t new_width = width();
  int32_t new_height = height();
  AdjustAlongAxis(rect.x(), rect.width(), &new_x, &new_width);
  AdjustAlongAxis(rect.y(), rect.height(), &new_y, &new_height);
  return Rect(new_x, new_y, new_width, new_height);
}

Point Rect::CenterPoint() const {
  return Point(x() + (width() + 1) / 2, y() + (height() + 1) / 2);
}

bool Rect::SharesEdgeWith(const Rect& rect) const {
  return (y() == rect.y() && height() == rect.height() &&
             (x() == rect.right() || right() == rect.x())) ||
         (x() == rect.x() && width() == rect.width() &&
             (y() == rect.bottom() || bottom() == rect.y()));
}

void FloatRect::Inset(float left, float top, float right, float bottom) {
  Offset(left, top);
  set_width(std::max<float>(width() - left - right, 0.0f));
  set_height(std::max<float>(height() - top - bottom, 0.0f));
}

void FloatRect::Offset(float horizontal, float vertical) {
  rect_.point.x += horizontal;
  rect_.point.y += vertical;
}

bool FloatRect::Contains(float point_x, float point_y) const {
  return (point_x >= x()) && (point_x < right()) &&
         (point_y >= y()) && (point_y < bottom());
}

bool FloatRect::Contains(const FloatRect& rect) const {
  return (rect.x() >= x() && rect.right() <= right() &&
          rect.y() >= y() && rect.bottom() <= bottom());
}

bool FloatRect::Intersects(const FloatRect& rect) const {
  return !(rect.x() >= right() || rect.right() <= x() ||
           rect.y() >= bottom() || rect.bottom() <= y());
}

FloatRect FloatRect::Intersect(const FloatRect& rect) const {
  float rx = std::max(x(), rect.x());
  float ry = std::max(y(), rect.y());
  float rr = std::min(right(), rect.right());
  float rb = std::min(bottom(), rect.bottom());

  if (rx >= rr || ry >= rb)
    rx = ry = rr = rb = 0;  // non-intersecting

  return FloatRect(rx, ry, rr - rx, rb - ry);
}

FloatRect FloatRect::Union(const FloatRect& rect) const {
  // special case empty rects...
  if (IsEmpty())
    return rect;
  if (rect.IsEmpty())
    return *this;

  float rx = std::min(x(), rect.x());
  float ry = std::min(y(), rect.y());
  float rr = std::max(right(), rect.right());
  float rb = std::max(bottom(), rect.bottom());

  return FloatRect(rx, ry, rr - rx, rb - ry);
}

FloatRect FloatRect::Subtract(const FloatRect& rect) const {
  // boundary cases:
  if (!Intersects(rect))
    return *this;
  if (rect.Contains(*this))
    return FloatRect();

  float rx = x();
  float ry = y();
  float rr = right();
  float rb = bottom();

  if (rect.y() <= y() && rect.bottom() >= bottom()) {
    // complete intersection in the y-direction
    if (rect.x() <= x()) {
      rx = rect.right();
    } else {
      rr = rect.x();
    }
  } else if (rect.x() <= x() && rect.right() >= right()) {
    // complete intersection in the x-direction
    if (rect.y() <= y()) {
      ry = rect.bottom();
    } else {
      rb = rect.y();
    }
  }
  return FloatRect(rx, ry, rr - rx, rb - ry);
}

FloatRect FloatRect::AdjustToFit(const FloatRect& rect) const {
  float new_x = x();
  float new_y = y();
  float new_width = width();
  float new_height = height();
  AdjustAlongAxis(rect.x(), rect.width(), &new_x, &new_width);
  AdjustAlongAxis(rect.y(), rect.height(), &new_y, &new_height);
  return FloatRect(new_x, new_y, new_width, new_height);
}

FloatPoint FloatRect::CenterPoint() const {
  return FloatPoint(x() + (width() + 1.0f) / 2.0f,
                    y() + (height() + 1.0f) / 2.0f);
}

bool FloatRect::SharesEdgeWith(const FloatRect& rect) const {
  return (y() == rect.y() && height() == rect.height() &&
             (x() == rect.right() || right() == rect.x())) ||
         (x() == rect.x() && width() == rect.width() &&
             (y() == rect.bottom() || bottom() == rect.y()));
}
}  // namespace pp
