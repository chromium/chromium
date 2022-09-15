// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_mouse_cursor.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool SetCursor(PP_Instance instance,
                  enum PP_MouseCursor_Type type,
                  PP_Resource image,
                  const struct PP_Point* hot_spot) {
  VLOG(4) << "PPB_MouseCursor::SetCursor()";
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetCursor(instance, type, image, hot_spot);
}

const PPB_MouseCursor_1_0 g_ppb_mousecursor_thunk_1_0 = {&SetCursor};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_MouseCursor_1_0* GetPPB_MouseCursor_1_0_Thunk() {
  return &g_ppb_mousecursor_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
