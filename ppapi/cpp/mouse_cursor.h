// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MOUSE_CURSOR_H_
#define PPAPI_CPP_MOUSE_CURSOR_H_

#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/point.h"

namespace pp {

class MouseCursor {
 public:
  /// Sets the given mouse cursor. The mouse cursor will be in effect whenever
  /// the mouse is over the given instance until it is set again by another
  /// call. Note that you can hide the mouse cursor by setting it to the
  /// <code>PP_MOUSECURSOR_TYPE_NONE</code> type.
  ///
  /// This function allows setting both system defined mouse cursors and
  /// custom cursors. To set a system-defined cursor, pass the type you want
  /// and set the custom image to a default-constructor ImageData object.
  /// To set a custom cursor, set the type to
  /// <code>PP_MOUSECURSOR_TYPE_CUSTOM</code> and specify your image and hot
  /// spot.
  ///
  /// @param[in] instance A handle identifying the instance that the mouse
  /// cursor will affect.
  ///
  /// @param[in] type A <code>PP_MouseCursor_Type</code> identifying the type
  /// of mouse cursor to show. See <code>ppapi/c/ppb_mouse_cursor.h</code>.
  ///
  /// @param[in] image A <code>ImageData</code> object identifying the
  /// custom image to set when the type is
  /// <code>PP_MOUSECURSOR_TYPE_CUSTOM</code>. The image must be less than 32
  /// pixels in each direction and must be of the system's native image format.
  /// When you are specifying a predefined cursor, this parameter should be a
  /// default-constructed ImageData.
  ///
  /// @param[in] hot_spot When setting a custom cursor, this identifies the
  /// pixel position within the given image of the "hot spot" of the cursor.
  /// When specifying a stock cursor, this parameter is ignored.
  ///
  /// @return true on success, or false if the instance or cursor type
  /// was invalid or if the image was too large.
  static bool SetCursor(const InstanceHandle& instance,
                        PP_MouseCursor_Type type,
                        const ImageData& image = ImageData(),
                        const Point& hot_spot = Point(0, 0));
};

}  // namespace pp

#endif  // PPAPI_CPP_MOUSE_CURSOR_H_
