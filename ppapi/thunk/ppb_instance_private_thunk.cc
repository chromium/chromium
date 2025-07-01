// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_instance_private.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

struct PP_Var GetWindowObject(PP_Instance instance) {
  VLOG(4) << "PPB_Instance_Private::GetWindowObject()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetWindowObject(instance);
}

struct PP_Var GetOwnerElementObject(PP_Instance instance) {
  VLOG(4) << "PPB_Instance_Private::GetOwnerElementObject()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetOwnerElementObject(instance);
}

struct PP_Var ExecuteScript(PP_Instance instance,
                            struct PP_Var script,
                            struct PP_Var* exception) {
  VLOG(4) << "PPB_Instance_Private::ExecuteScript()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->ExecuteScript(instance, script, exception);
}

const PPB_Instance_Private_0_1 g_ppb_instance_private_thunk_0_1 = {
    &GetWindowObject, &GetOwnerElementObject, &ExecuteScript};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Instance_Private_0_1*
GetPPB_Instance_Private_0_1_Thunk() {
  return &g_ppb_instance_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
