// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_CURSOR_CONTROL_DEV_H_
#define PPAPI_CPP_DEV_CURSOR_CONTROL_DEV_H_

#include "ppapi/c/dev/ppb_cursor_control_dev.h"

/// @file
/// This file defines APIs for controlling the cursor.

namespace pp {

class ImageData;
class InstanceHandle;
class Point;

/// APIs for controlling the cursor.
class CursorControl_Dev {
 public:
  CursorControl_Dev() {}

  bool SetCursor(const InstanceHandle& instance,
                 PP_CursorType_Dev type,
                 const ImageData& custom_image,
                 const Point& hot_spot);
  bool LockCursor(const InstanceHandle& instance);
  bool UnlockCursor(const InstanceHandle& instance);
  bool HasCursorLock(const InstanceHandle& instance);
  bool CanLockCursor(const InstanceHandle& instance);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_CURSOR_CONTROL_DEV_H_
