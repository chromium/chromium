// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_gamepad.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_gamepad_api.h"

namespace ppapi {
namespace thunk {

namespace {

void Sample(PP_Instance instance, struct PP_GamepadsSampleData* data) {
  VLOG(4) << "PPB_Gamepad::Sample()";
  EnterInstanceAPI<PPB_Gamepad_API> enter(instance);
  if (enter.failed()) {
    memset(data, 0, sizeof(*data));
    return;
  }
  enter.functions()->Sample(instance, data);
}

const PPB_Gamepad_1_0 g_ppb_gamepad_thunk_1_0 = {&Sample};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Gamepad_1_0* GetPPB_Gamepad_1_0_Thunk() {
  return &g_ppb_gamepad_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
