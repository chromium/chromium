/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_touch_point.idl modified Thu Mar 28 10:13:07 2013. */

#ifndef PPAPI_C_PP_TOUCH_POINT_H_
#define PPAPI_C_PP_TOUCH_POINT_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the API to create a touch point or position where fingers
 * makes contact with touch screen device.
 */


/**
 * @addtogroup Structs
 * @{
 */
/**
 * The <code>PP_TouchPoint</code> struct represents all information about a
 * single touch point, such as position, id, rotation angle, and pressure.
 */
struct PP_TouchPoint {
  /**
   * This value represents the identifier for this TouchPoint. The id
   * corresponds to the order in which the points were pressed. For example,
   * the first point to be pressed has an id of 0, the second has an id of 1,
   * and so on. An id can be reused when a touch point is released.  For
   * example, if two fingers are down, with id 0 and 1, and finger 0 releases,
   * the next finger to be pressed can be assigned to id 0.
   */
  uint32_t id;
  /**
   * This value represents the x and y pixel position of this TouchPoint
   * relative to the upper-left of the module instance receiving the event.
   */
  struct PP_FloatPoint position;
  /**
   * This value represents the elliptical radii, in screen pixels, in the x
   * and y direction of this TouchPoint.
   */
  struct PP_FloatPoint radius;
  /**
   * This value represents the angle of rotation in degrees of the elliptical
   * model of this TouchPoint clockwise from "up."
   */
  float rotation_angle;
  /**
   * This value represents the pressure applied to this TouchPoint.  This value
   * is typically between 0 and 1, with 0 indicating no pressure and 1
   * indicating some maximum pressure. Scaling differs depending on the
   * hardware and the value is not guaranteed to stay within that range.
   */
  float pressure;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_TouchPoint, 28);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_MakeTouchPoint() creates a <code>PP_TouchPoint</code>.
 *
 * @return A <code>PP_TouchPoint</code> structure.
 */
PP_INLINE struct PP_TouchPoint PP_MakeTouchPoint(void) {
  struct PP_TouchPoint result = { 0, {0, 0}, {0, 0}, 0, 0 };
  return result;
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_TOUCH_POINT_H_ */

