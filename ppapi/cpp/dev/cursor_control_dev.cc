// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/cursor_control_dev.h"

#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_CursorControl_Dev_0_4>() {
  return PPB_CURSOR_CONTROL_DEV_INTERFACE_0_4;
}

}  // namespace

bool CursorControl_Dev::SetCursor(const InstanceHandle& instance,
                                  PP_CursorType_Dev type,
                                  const ImageData& custom_image,
                                  const Point& hot_spot) {
  if (has_interface<PPB_CursorControl_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_CursorControl_Dev_0_4>()->SetCursor(
        instance.pp_instance(), type, custom_image.pp_resource(),
        &hot_spot.pp_point()));
  }
  return false;
}

bool LockCursor(const InstanceHandle& instance) {
  if (has_interface<PPB_CursorControl_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_CursorControl_Dev_0_4>()->LockCursor(
        instance.pp_instance()));
  }
  return false;
}

bool UnlockCursor(const InstanceHandle& instance) {
  if (has_interface<PPB_CursorControl_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_CursorControl_Dev_0_4>()->UnlockCursor(
        instance.pp_instance()));
  }
  return false;
}

bool HasCursorLock(const InstanceHandle& instance) {
  if (has_interface<PPB_CursorControl_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_CursorControl_Dev_0_4>()->HasCursorLock(
        instance.pp_instance()));
  }
  return false;
}

bool CanLockCursor(const InstanceHandle& instance) {
  if (has_interface<PPB_CursorControl_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_CursorControl_Dev_0_4>()->CanLockCursor(
        instance.pp_instance()));
  }
  return false;
}

}  // namespace pp
