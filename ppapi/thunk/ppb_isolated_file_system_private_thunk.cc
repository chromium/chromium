// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_isolated_file_system_private.idl modified Tue Dec  3
// 11:01:20 2013.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_isolated_file_system_private_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t Open(PP_Instance instance,
             PP_IsolatedFileSystemType_Private type,
             PP_Resource* file_system,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_IsolatedFileSystem_Private::Open()";
  EnterInstanceAPI<PPB_IsolatedFileSystem_Private_API> enter(instance,
                                                             callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.functions()->Open(instance, type, file_system, enter.callback()));
}

const PPB_IsolatedFileSystem_Private_0_2
    g_ppb_isolatedfilesystem_private_thunk_0_2 = {&Open};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_IsolatedFileSystem_Private_0_2*
GetPPB_IsolatedFileSystem_Private_0_2_Thunk() {
  return &g_ppb_isolatedfilesystem_private_thunk_0_2;
}

}  // namespace thunk
}  // namespace ppapi
