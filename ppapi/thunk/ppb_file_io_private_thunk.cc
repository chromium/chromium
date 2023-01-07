// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_file_io_private.idl modified Tue Mar 26 15:29:46 2013.

#include <stdint.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_io_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t RequestOSFileHandle(PP_Resource file_io,
                            PP_FileHandle* handle,
                            struct PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->RequestOSFileHandle(
      handle,
      enter.callback()));
}

const PPB_FileIO_Private_0_1 g_ppb_fileio_private_thunk_0_1 = {
  &RequestOSFileHandle
};

}  // namespace

const PPB_FileIO_Private_0_1* GetPPB_FileIO_Private_0_1_Thunk() {
  return &g_ppb_fileio_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
