// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_instance.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) {
  VLOG(4) << "PPB_Instance::BindGraphics()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->BindGraphics(instance, device);
}

PP_Bool IsFullFrame(PP_Instance instance) {
  VLOG(4) << "PPB_Instance::IsFullFrame()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFullFrame(instance);
}

const PPB_Instance_1_0 g_ppb_instance_thunk_1_0 = {&BindGraphics, &IsFullFrame};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Instance_1_0* GetPPB_Instance_1_0_Thunk() {
  return &g_ppb_instance_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
