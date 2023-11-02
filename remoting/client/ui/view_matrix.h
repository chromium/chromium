// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_UI_VIEW_MATRIX_H_
#define REMOTING_CLIENT_UI_VIEW_MATRIX_H_

#include <array>

namespace remoting {

// A 2D non-skew equally scaled transformation matrix.
// | SCALE, 0,     OFFSET_X, |
// | 0,     SCALE, OFFSET_Y, |
// | 0,     0,     1         |
class ViewMatrix {
 public:
  struct Vector2D {
    float x;
    float y;
  };

  // Same as Vector2D. This alias just serves as a context hint.
  using Point = Vector2D;

  // Creates an empty matrix (0 scale and offsets).
  ViewMatrix();

  ViewMatrix(float scale, const Vector2D& offset);

  ~ViewMatrix();

  // Applies the matrix on the point and returns the result.
  Point MapPoint(const Point& point) const;

  // Applies the matrix on the vector and returns the result. This only scales
  // the vector and does not apply offset.
  Vector2D MapVector(const Vector2D& vector) const;

  // Sets the scale factor, with the pivot point at (0, 0). This WON'T affect
  // the offset.
  void SetScale(float scale);

  // Returns the scale of this matrix.
  float GetScale() const;

  // Sets the offset.
  void SetOffset(const Point& offset);

  const Vector2D& GetOffset() const;

  // Adjust the matrix M to M' such that:
  // M * p_a = p_b => M' * p_a = scale * (p_b - pivot) + pivot
  void PostScale(const Point& pivot, float scale);

  // Applies translation to the matrix.
  // M * p_a = p_b => M' * p_a = p_b + delta
  void PostTranslate(const Vector2D& delta);

  // Returns the inverse of this matrix.
  ViewMatrix Invert() const;

  // Returns true if the scale and offsets are both 0.
  bool IsEmpty() const;

  // Converts to the 3x3 matrix array.
  std::array<float, 9> ToMatrixArray() const;

 private:
  float scale_;
  Vector2D offset_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_UI_VIEW_MATRIX_H_
