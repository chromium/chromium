// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_fullscreen.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsFullscreen(PP_Instance instance) {
  VLOG(4) << "PPB_Fullscreen::IsFullscreen()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFullscreen(instance);
}

PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) {
  VLOG(4) << "PPB_Fullscreen::SetFullscreen()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetFullscreen(instance, fullscreen);
}

PP_Bool GetScreenSize(PP_Instance instance, struct PP_Size* size) {
  VLOG(4) << "PPB_Fullscreen::GetScreenSize()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->GetScreenSize(instance, size);
}

const PPB_Fullscreen_1_0 g_ppb_fullscreen_thunk_1_0 = {
    &IsFullscreen, &SetFullscreen, &GetScreenSize};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Fullscreen_1_0* GetPPB_Fullscreen_1_0_Thunk() {
  return &g_ppb_fullscreen_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
