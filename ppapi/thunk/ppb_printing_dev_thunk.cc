// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_printing_dev.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_printing_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_Printing_Dev::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreatePrinting(instance);
}

int32_t GetDefaultPrintSettings(PP_Resource resource,
                                struct PP_PrintSettings_Dev* print_settings,
                                struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Printing_Dev::GetDefaultPrintSettings()";
  EnterResource<PPB_Printing_API> enter(resource, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetDefaultPrintSettings(
      print_settings, enter.callback()));
}

const PPB_Printing_Dev_0_7 g_ppb_printing_dev_thunk_0_7 = {
    &Create, &GetDefaultPrintSettings};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Printing_Dev_0_7* GetPPB_Printing_Dev_0_7_Thunk() {
  return &g_ppb_printing_dev_thunk_0_7;
}

}  // namespace thunk
}  // namespace ppapi
