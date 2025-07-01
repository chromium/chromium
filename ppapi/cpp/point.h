// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_POINT_H_
#define PPAPI_CPP_POINT_H_

#include <stdint.h>

#include "ppapi/c/pp_point.h"

/// @file
/// This file defines the API to create a 2 dimensional point.

namespace pp {

/// A 2 dimensional point with 0,0 being the upper-left starting coordinate.
class Point {
 public:
  /// The default constructor for a point at 0,0.
  Point() {
    point_.x = 0;
    point_.y = 0;
  }

  /// A constructor accepting two int32_t values for x and y and converting
  /// them to a Point.
  ///
  /// @param[in] in_x An int32_t value representing a horizontal coordinate
  /// of a point, starting with 0 as the left-most coordinate.
  /// @param[in] in_y An int32_t value representing a vertical coordinate
  /// of a point, starting with 0 as the top-most coordinate.
  Point(int32_t in_x, int32_t in_y) {
    point_.x = in_x;
    point_.y = in_y;
  }

  /// A constructor accepting a pointer to a PP_Point and converting the
  /// PP_Point to a Point. This is an implicit conversion constructor.
  ///
  /// @param[in] point A pointer to a PP_Point.
  Point(const PP_Point& point) {  // Implicit.
    point_.x = point.x;
    point_.y = point.y;
  }

  /// Destructor.
  ~Point() {
  }

  /// A function allowing implicit conversion of a Point to a PP_Point.
  /// @return A Point.
  operator PP_Point() const {
    return point_;
  }

  /// Getter function for returning the internal PP_Point struct.
  ///
  /// @return A const reference to the internal PP_Point struct.
  const PP_Point& pp_point() const {
    return point_;
  }

  /// Getter function for returning the internal PP_Point struct.
  ///
  /// @return A mutable reference to the PP_Point struct.
  PP_Point& pp_point() {
    return point_;
  }

  /// Getter function for returning the value of x.
  ///
  /// @return The value of x for this Point.
  int32_t x() const { return point_.x; }

  /// Setter function for setting the value of x.
  ///
  /// @param[in] in_x A new x value.
  void set_x(int32_t in_x) {
    point_.x = in_x;
  }

  /// Getter function for returning the value of y.
  ///
  /// @return The value of y for this Point.
  int32_t y() const { return point_.y; }

  /// Setter function for setting the value of y.
  ///
  /// @param[in] in_y A new y value.
  void set_y(int32_t in_y) {
    point_.y = in_y;
  }

  /// Adds two Points (this and other) together by adding their x values and
  /// y values.
  ///
  /// @param[in] other A Point.
  ///
  /// @return A new Point containing the result.
  Point operator+(const Point& other) const {
    return Point(x() + other.x(), y() + other.y());
  }

  /// Subtracts one Point from another Point by subtracting their x values
  /// and y values. Returns a new point with the result.
  ///
  /// @param[in] other A Point.
  ///
  /// @return A new Point containing the result.
  Point operator-(const Point& other) const {
    return Point(x() - other.x(), y() - other.y());
  }

  /// Adds two Points (this and other) together by adding their x and y
  /// values. Returns this point as the result.
  ///
  /// @param[in] other A Point.
  ///
  /// @return This Point containing the result.
  Point& operator+=(const Point& other) {
    point_.x += other.x();
    point_.y += other.y();
    return *this;
  }

  /// Subtracts one Point from another Point by subtracting their x values
  /// and y values. Returns this point as the result.
  ///
  /// @param[in] other A Point.
  ///
  /// @return This Point containing the result.
  Point& operator-=(const Point& other) {
    point_.x -= other.x();
    point_.y -= other.y();
    return *this;
  }

  /// Swaps the coordinates of two Points.
  ///
  /// @param[in] other A Point.
  void swap(Point& other) {
    int32_t x = point_.x;
    int32_t y = point_.y;
    point_.x = other.point_.x;
    point_.y = other.point_.y;
    other.point_.x = x;
    other.point_.y = y;
  }

 private:
  PP_Point point_;
};

/// A 2 dimensional floating-point point with 0,0 being the upper-left starting
/// coordinate.
class FloatPoint {
 public:
  /// A constructor for a point at 0,0.
  FloatPoint() {
    float_point_.x = 0.0f;
    float_point_.y = 0.0f;
  }

  /// A constructor accepting two values for x and y and converting them to a
  /// FloatPoint.
  ///
  /// @param[in] in_x An value representing a horizontal coordinate of a
  /// point, starting with 0 as the left-most coordinate.
  ///
  /// @param[in] in_y An value representing a vertical coordinate of a point,
  /// starting with 0 as the top-most coordinate.
  FloatPoint(float in_x, float in_y) {
    float_point_.x = in_x;
    float_point_.y = in_y;
  }

  /// A constructor accepting a pointer to a PP_FloatPoint and converting the
  /// PP_Point to a Point. This is an implicit conversion constructor.
  ///
  /// @param[in] point A PP_FloatPoint.
  FloatPoint(const PP_FloatPoint& point) {  // Implicit.
    float_point_.x = point.x;
    float_point_.y = point.y;
  }
  /// Destructor.
  ~FloatPoint() {
  }

  /// A function allowing implicit conversion of a FloatPoint to a
  /// PP_FloatPoint.
  operator PP_FloatPoint() const {
    return float_point_;
  }

  /// Getter function for returning the internal PP_FloatPoint struct.
  ///
  /// @return A const reference to the internal PP_FloatPoint struct.
  const PP_FloatPoint& pp_float_point() const {
    return float_point_;
  }

  /// Getter function for returning the internal PP_Point struct.
  ///
  /// @return A mutable reference to the PP_Point struct.
  PP_FloatPoint& pp_float_point() {
    return float_point_;
  }

  /// Getter function for returning the value of x.
  ///
  /// @return The value of x for this Point.
  float x() const { return float_point_.x; }

  /// Setter function for setting the value of x.
  ///
  /// @param[in] in_x A new x value.
  void set_x(float in_x) {
    float_point_.x = in_x;
  }

  /// Getter function for returning the value of y.
  ///
  /// @return The value of y for this Point.
  float y() const { return float_point_.y; }

  /// Setter function for setting the value of y.
  ///
  /// @param[in] in_y A new y value.
  void set_y(float in_y) {
    float_point_.y = in_y;
  }

  /// Adds two Points (this and other) together by adding their x values and
  /// y values.
  ///
  /// @param[in] other A Point.
  ///
  /// @return A new Point containing the result.
  FloatPoint operator+(const FloatPoint& other) const {
    return FloatPoint(x() + other.x(), y() + other.y());
  }

  /// Subtracts one Point from another Point by subtracting their x values
  /// and y values. Returns a new point with the result.
  ///
  /// @param[in] other A FloatPoint.
  ///
  /// @return A new Point containing the result.
  FloatPoint operator-(const FloatPoint& other) const {
    return FloatPoint(x() - other.x(), y() - other.y());
  }

  /// Adds two Points (this and other) together by adding their x and y
  /// values. Returns this point as the result.
  ///
  /// @param[in] other A Point.
  ///
  /// @return This Point containing the result.
  FloatPoint& operator+=(const FloatPoint& other) {
    float_point_.x += other.x();
    float_point_.y += other.y();
    return *this;
  }

  /// Subtracts one Point from another Point by subtracting their x values
  /// and y values. Returns this point as the result.
  ///
  /// @param[in] other A Point.
  ///
  /// @return This Point containing the result.
  FloatPoint& operator-=(const FloatPoint& other) {
    float_point_.x -= other.x();
    float_point_.y -= other.y();
    return *this;
  }

  /// Swaps the coordinates of two Points.
  ///
  /// @param[in] other A Point.
  void swap(FloatPoint& other) {
    float x = float_point_.x;
    float y = float_point_.y;
    float_point_.x = other.float_point_.x;
    float_point_.y = other.float_point_.y;
    other.float_point_.x = x;
    other.float_point_.y = y;
  }

 private:
  PP_FloatPoint float_point_;
};

}  // namespace pp

/// Determines whether the x and y values of two Points are equal.
///
/// @param[in] lhs The Point on the left-hand side of the equation.
/// @param[in] rhs The Point on the right-hand side of the equation.
///
/// @return true if they are equal, false if unequal.
inline bool operator==(const pp::Point& lhs, const pp::Point& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

/// Determines whether two Points have different coordinates.
///
/// @param[in] lhs The Point on the left-hand side of the equation.
/// @param[in] rhs The Point on the right-hand side of the equation.
///
/// @return true if the coordinates of lhs are equal to the coordinates
/// of rhs, otherwise false.
inline bool operator!=(const pp::Point& lhs, const pp::Point& rhs) {
  return !(lhs == rhs);
}

/// Determines whether the x and y values of two FloatPoints are equal.
///
/// @param[in] lhs The Point on the left-hand side of the equation.
/// @param[in] rhs The Point on the right-hand side of the equation.
///
/// @return true if they are equal, false if unequal.
inline bool operator==(const pp::FloatPoint& lhs, const pp::FloatPoint& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

/// Determines whether two Points have different coordinates.
///
/// @param[in] lhs The Point on the left-hand side of the equation.
/// @param[in] rhs The Point on the right-hand side of the equation.
///
/// @return true if the coordinates of lhs are equal to the coordinates
/// of rhs, otherwise false.
inline bool operator!=(const pp::FloatPoint& lhs, const pp::FloatPoint& rhs) {
  return !(lhs == rhs);
}

#endif  // PPAPI_CPP_POINT_H_
