/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_mouse_cursor.idl modified Thu Mar 28 10:11:32 2013. */

#ifndef PPAPI_C_PPB_MOUSE_CURSOR_H_
#define PPAPI_C_PPB_MOUSE_CURSOR_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_MOUSECURSOR_INTERFACE_1_0 "PPB_MouseCursor;1.0"
#define PPB_MOUSECURSOR_INTERFACE PPB_MOUSECURSOR_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_MouseCursor</code> interface for setting
 * the mouse cursor.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_MouseCursor_Type</code> enumeration lists the available stock
 * cursor types.
 */
enum PP_MouseCursor_Type {
  PP_MOUSECURSOR_TYPE_CUSTOM = -1,
  PP_MOUSECURSOR_TYPE_POINTER = 0,
  PP_MOUSECURSOR_TYPE_CROSS = 1,
  PP_MOUSECURSOR_TYPE_HAND = 2,
  PP_MOUSECURSOR_TYPE_IBEAM = 3,
  PP_MOUSECURSOR_TYPE_WAIT = 4,
  PP_MOUSECURSOR_TYPE_HELP = 5,
  PP_MOUSECURSOR_TYPE_EASTRESIZE = 6,
  PP_MOUSECURSOR_TYPE_NORTHRESIZE = 7,
  PP_MOUSECURSOR_TYPE_NORTHEASTRESIZE = 8,
  PP_MOUSECURSOR_TYPE_NORTHWESTRESIZE = 9,
  PP_MOUSECURSOR_TYPE_SOUTHRESIZE = 10,
  PP_MOUSECURSOR_TYPE_SOUTHEASTRESIZE = 11,
  PP_MOUSECURSOR_TYPE_SOUTHWESTRESIZE = 12,
  PP_MOUSECURSOR_TYPE_WESTRESIZE = 13,
  PP_MOUSECURSOR_TYPE_NORTHSOUTHRESIZE = 14,
  PP_MOUSECURSOR_TYPE_EASTWESTRESIZE = 15,
  PP_MOUSECURSOR_TYPE_NORTHEASTSOUTHWESTRESIZE = 16,
  PP_MOUSECURSOR_TYPE_NORTHWESTSOUTHEASTRESIZE = 17,
  PP_MOUSECURSOR_TYPE_COLUMNRESIZE = 18,
  PP_MOUSECURSOR_TYPE_ROWRESIZE = 19,
  PP_MOUSECURSOR_TYPE_MIDDLEPANNING = 20,
  PP_MOUSECURSOR_TYPE_EASTPANNING = 21,
  PP_MOUSECURSOR_TYPE_NORTHPANNING = 22,
  PP_MOUSECURSOR_TYPE_NORTHEASTPANNING = 23,
  PP_MOUSECURSOR_TYPE_NORTHWESTPANNING = 24,
  PP_MOUSECURSOR_TYPE_SOUTHPANNING = 25,
  PP_MOUSECURSOR_TYPE_SOUTHEASTPANNING = 26,
  PP_MOUSECURSOR_TYPE_SOUTHWESTPANNING = 27,
  PP_MOUSECURSOR_TYPE_WESTPANNING = 28,
  PP_MOUSECURSOR_TYPE_MOVE = 29,
  PP_MOUSECURSOR_TYPE_VERTICALTEXT = 30,
  PP_MOUSECURSOR_TYPE_CELL = 31,
  PP_MOUSECURSOR_TYPE_CONTEXTMENU = 32,
  PP_MOUSECURSOR_TYPE_ALIAS = 33,
  PP_MOUSECURSOR_TYPE_PROGRESS = 34,
  PP_MOUSECURSOR_TYPE_NODROP = 35,
  PP_MOUSECURSOR_TYPE_COPY = 36,
  PP_MOUSECURSOR_TYPE_NONE = 37,
  PP_MOUSECURSOR_TYPE_NOTALLOWED = 38,
  PP_MOUSECURSOR_TYPE_ZOOMIN = 39,
  PP_MOUSECURSOR_TYPE_ZOOMOUT = 40,
  PP_MOUSECURSOR_TYPE_GRAB = 41,
  PP_MOUSECURSOR_TYPE_GRABBING = 42,
  PP_MOUSECURSOR_TYPE_MIDDLEPANNINGVERTICAL = 43,
  PP_MOUSECURSOR_TYPE_MIDDLEPANNINGHORIZONTAL = 44
};
PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(PP_MouseCursor_Type, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_MouseCursor</code> allows setting the mouse cursor.
 */
struct PPB_MouseCursor_1_0 {
  /**
   * Sets the given mouse cursor. The mouse cursor will be in effect whenever
   * the mouse is over the given instance until it is set again by another
   * call. Note that you can hide the mouse cursor by setting it to the
   * <code>PP_MOUSECURSOR_TYPE_NONE</code> type.
   *
   * This function allows setting both system defined mouse cursors and
   * custom cursors. To set a system-defined cursor, pass the type you want
   * and set the custom image to 0 and the hot spot to NULL. To set a custom
   * cursor, set the type to <code>PP_MOUSECURSOR_TYPE_CUSTOM</code> and
   * specify your image and hot spot.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying the instance
   * that the mouse cursor will affect.
   *
   * @param[in] type A <code>PP_MouseCursor_Type</code> identifying the type of
   * mouse cursor to show.
   *
   * @param[in] image A <code>PPB_ImageData</code> resource identifying the
   * custom image to set when the type is
   * <code>PP_MOUSECURSOR_TYPE_CUSTOM</code>. The image must be less than 32
   * pixels in each direction and must be of the system's native image format.
   * When you are specifying a predefined cursor, this parameter must be 0.
   *
   * @param[in] hot_spot When setting a custom cursor, this identifies the
   * pixel position within the given image of the "hot spot" of the cursor.
   * When specifying a stock cursor, this parameter is ignored.
   *
   * @return PP_TRUE on success, or PP_FALSE if the instance or cursor type
   * is invalid, or if the image is too large.
   */
  PP_Bool (*SetCursor)(PP_Instance instance,
                       enum PP_MouseCursor_Type type,
                       PP_Resource image,
                       const struct PP_Point* hot_spot);
};

typedef struct PPB_MouseCursor_1_0 PPB_MouseCursor;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MOUSE_CURSOR_H_ */

