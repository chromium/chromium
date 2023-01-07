// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_isolated_file_system_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t Open(PP_Instance instance,
             PP_Resource* file_system,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Ext_CrxFileSystem_Private::Open()";
  EnterInstanceAPI<PPB_IsolatedFileSystem_Private_API> enter(instance,
                                                             callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.functions()->Open(
      instance,
      PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX,
      file_system,
      enter.callback()));
}

const PPB_Ext_CrxFileSystem_Private_0_1
    g_ppb_ext_crxfilesystem_private_thunk_0_1 = {
  &Open
};

}  // namespace

const PPB_Ext_CrxFileSystem_Private_0_1*
    GetPPB_Ext_CrxFileSystem_Private_0_1_Thunk() {
  return &g_ppb_ext_crxfilesystem_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
