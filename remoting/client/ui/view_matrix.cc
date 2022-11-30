// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ui/view_matrix.h"

namespace remoting {

ViewMatrix::ViewMatrix() : ViewMatrix(0.f, {0.f, 0.f}) {}

ViewMatrix::ViewMatrix(float scale, const Vector2D& offset)
    : scale_(scale), offset_(offset) {}

ViewMatrix::~ViewMatrix() = default;

ViewMatrix::Point ViewMatrix::MapPoint(const Point& point) const {
  float x = scale_ * point.x + offset_.x;
  float y = scale_ * point.y + offset_.y;
  return {x, y};
}

ViewMatrix::Vector2D ViewMatrix::MapVector(const Vector2D& vector) const {
  float x = scale_ * vector.x;
  float y = scale_ * vector.y;
  return {x, y};
}

void ViewMatrix::SetScale(float scale) {
  scale_ = scale;
}

float ViewMatrix::GetScale() const {
  return scale_;
}

void ViewMatrix::SetOffset(const Point& offset) {
  offset_ = offset;
}

const ViewMatrix::Vector2D& ViewMatrix::GetOffset() const {
  return offset_;
}

void ViewMatrix::PostScale(const Point& pivot, float scale) {
  scale_ *= scale;
  offset_.x *= scale;
  offset_.x += (1.f - scale) * pivot.x;
  offset_.y *= scale;
  offset_.y += (1.f - scale) * pivot.y;
}

void ViewMatrix::PostTranslate(const Vector2D& delta) {
  offset_.x += delta.x;
  offset_.y += delta.y;
}

ViewMatrix ViewMatrix::Invert() const {
  return ViewMatrix(1.f / scale_, {-offset_.x / scale_, -offset_.y / scale_});
}

std::array<float, 9> ViewMatrix::ToMatrixArray() const {
  return {{scale_, 0, offset_.x,  // Row 1
           0, scale_, offset_.y,  // Row 2
           0, 0, 1}};
}

bool ViewMatrix::IsEmpty() const {
  return scale_ == 0.f && offset_.x == 0.f && offset_.y == 0.f;
}

}  // namespace remoting
