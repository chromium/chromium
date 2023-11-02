// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_view_dev.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_view_api.h"

namespace ppapi {
namespace thunk {

namespace {

float GetDeviceScale(PP_Resource resource) {
  VLOG(4) << "PPB_View_Dev::GetDeviceScale()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetDeviceScale();
}

float GetCSSScale(PP_Resource resource) {
  VLOG(4) << "PPB_View_Dev::GetCSSScale()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetCSSScale();
}

const PPB_View_Dev_0_1 g_ppb_view_dev_thunk_0_1 = {&GetDeviceScale,
                                                   &GetCSSScale};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_View_Dev_0_1* GetPPB_View_Dev_0_1_Thunk() {
  return &g_ppb_view_dev_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
