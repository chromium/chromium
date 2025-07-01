// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

// This interface is only for temporary backwards compat and currently just
// forwards to the stable interfaces that implement these features.

namespace ppapi {
namespace thunk {

namespace {

PP_Bool SetCursor(PP_Instance instance,
                  PP_CursorType_Dev type,
                  PP_Resource custom_image,
                  const PP_Point* hot_spot) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetCursor(instance,
      static_cast<PP_MouseCursor_Type>(type), custom_image, hot_spot);
}

PP_Bool LockCursor(PP_Instance instance) {
  return PP_FALSE;
}

PP_Bool UnlockCursor(PP_Instance instance) {
  return PP_FALSE;
}

PP_Bool HasCursorLock(PP_Instance instance) {
  return PP_FALSE;
}

PP_Bool CanLockCursor(PP_Instance instance) {
  return PP_FALSE;
}

const PPB_CursorControl_Dev g_ppb_cursor_control_thunk = {
  &SetCursor,
  &LockCursor,
  &UnlockCursor,
  &HasCursorLock,
  &CanLockCursor
};

}  // namespace

const PPB_CursorControl_Dev_0_4* GetPPB_CursorControl_Dev_0_4_Thunk() {
  return &g_ppb_cursor_control_thunk;
}

}  // namespace thunk
}  // namespace ppapi
