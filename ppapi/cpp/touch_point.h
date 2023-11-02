// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_TOUCH_POINT_H_
#define PPAPI_CPP_TOUCH_POINT_H_

#include <stdint.h>

#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/point.h"

namespace pp {

/// Wrapper class for PP_TouchPoint.
class TouchPoint {
 public:
  TouchPoint() : touch_point_(PP_MakeTouchPoint()) {}

  TouchPoint(const PP_TouchPoint& point)
      : touch_point_(point), tilt_(PP_MakeFloatPoint(0, 0)) {}

  TouchPoint(const PP_TouchPoint& point, const PP_FloatPoint& tilt)
      : touch_point_(point), tilt_(tilt) {}

  /// @return The identifier for this TouchPoint. This corresponds to the order
  /// in which the points were pressed. For example, the first point to be
  /// pressed has an id of 0, the second has an id of 1, and so on. An id can be
  /// reused when a touch point is released.  For example, if two fingers are
  /// down, with id 0 and 1, and finger 0 releases, the next finger to be
  /// pressed can be assigned to id 0.
  uint32_t id() const { return touch_point_.id; }

  /// @return The x-y coordinates of this TouchPoint, in DOM coordinate space.
  FloatPoint position() const {
    return pp::FloatPoint(touch_point_.position);
  }

  /// @return The elliptical radii, in screen pixels, in the x and y direction
  /// of this TouchPoint.
  FloatPoint radii() const { return pp::FloatPoint(touch_point_.radius); }

  /// @return The angle of rotation of the elliptical model of this TouchPoint
  /// from the y-axis.
  float rotation_angle() const { return touch_point_.rotation_angle; }

  /// @return The pressure applied to this TouchPoint.  This is typically a
  /// value between 0 and 1, with 0 indicating no pressure and 1 indicating
  /// some maximum pressure, but scaling differs depending on the hardware and
  /// the value is not guaranteed to stay within that range.
  float pressure() const { return touch_point_.pressure; }

  /// @return The tilt of this touchpoint. This is a float point. Values of x
  /// and y are between 0 and 90, with 0 indicating 0 degrees and 90 indicating
  //  90 degrees.
  PP_FloatPoint tilt() const { return tilt_; }

 private:
  PP_TouchPoint touch_point_;
  PP_FloatPoint tilt_;
};

}  // namespace pp

#endif  // PPAPI_CPP_TOUCH_POINT_H_
