// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_DEMO_FLOCK_VECTOR2_H_
#define EXAMPLES_DEMO_FLOCK_VECTOR2_H_

#include <stdlib.h>
#include <cmath>
#include <limits>

// A small class that encapsulates a 2D vector.  Provides a few simple
// operations.

class Vector2 {
 public:
  Vector2() : x_(0.0), y_(0.0) {}
  Vector2(double x, double y) : x_(x), y_(y) {}
  ~Vector2() {}

  // Create a new vector that represents a - b.
  static Vector2 Difference(const Vector2& a, const Vector2& b) {
    Vector2 diff(a.x() - b.x(), a.y() - b.y());
    return diff;
  }

  // The magnitude of this vector.
  double Magnitude() const {
    return sqrt(x_ * x_ + y_ * y_);
  }

  // Add |vec| to this vector.  Works in-place.
  void Add(const Vector2& vec) {
    x_ += vec.x();
    y_ += vec.y();
  }

  // Normalize this vector in-place.  If the vector is degenerate (size 0)
  // then do nothing.
  void Normalize() {
    double mag = Magnitude();
    if (fabs(mag) < std::numeric_limits<double>::epsilon())
      return;
    Scale(1.0 / mag);
  }

  // Scale the vector in-place by |scale|.
  void Scale(double scale) {
    x_ *= scale;
    y_ *= scale;
  }

  // Clamp a vector to a maximum magnitude.  Works on the vector in-place.
  // @param max_mag The maximum magnitude of the vector.
  void Clamp(double max_mag) {
    double mag = Magnitude();
    if (mag > max_mag) {
      Scale(max_mag / mag);  // Does Normalize() followed by Scale(max_mag).
    }
  }

  // Compute the "heading" of a vector - this is the angle in radians between
  // the vector and the x-axis.
  // @return {!number} The "heading" angle in radians.
  double Heading() const {
    double angle = atan2(y_, x_);
    return angle;
  }

  // Accessors and mutators for the coordinate values.
  double x() const { return x_; }
  void set_x(double x) { x_ = x; }

  double y() const { return y_; }
  void set_y(double y) { y_ = y; }

  double x_;
  double y_;
};

#endif  // EXAMPLES_DEMO_FLOCK_VECTOR2_H_
