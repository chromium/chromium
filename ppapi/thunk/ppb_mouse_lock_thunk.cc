// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_mouse_lock.idl modified Wed May 15 13:57:07 2013.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t LockMouse(PP_Instance instance, struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_MouseLock::LockMouse()";
  EnterInstance enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.functions()->LockMouse(instance, enter.callback()));
}

void UnlockMouse(PP_Instance instance) {
  VLOG(4) << "PPB_MouseLock::UnlockMouse()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->UnlockMouse(instance);
}

const PPB_MouseLock_1_0 g_ppb_mouselock_thunk_1_0 = {&LockMouse, &UnlockMouse};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_MouseLock_1_0* GetPPB_MouseLock_1_0_Thunk() {
  return &g_ppb_mouselock_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
