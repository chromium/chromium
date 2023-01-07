// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From trusted/ppb_file_chooser_trusted.idl modified Fri Feb  7 08:29:41 2014.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t ShowWithoutUserGesture_0_5(PP_Resource chooser,
                                   PP_Bool save_as,
                                   struct PP_Var suggested_file_name,
                                   struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileChooserTrusted::ShowWithoutUserGesture_0_5()";
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ShowWithoutUserGesture0_5(
      save_as, suggested_file_name, enter.callback()));
}

int32_t ShowWithoutUserGesture(PP_Resource chooser,
                               PP_Bool save_as,
                               struct PP_Var suggested_file_name,
                               struct PP_ArrayOutput output,
                               struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileChooserTrusted::ShowWithoutUserGesture()";
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ShowWithoutUserGesture(
      save_as, suggested_file_name, output, enter.callback()));
}

const PPB_FileChooserTrusted_0_5 g_ppb_filechoosertrusted_thunk_0_5 = {
    &ShowWithoutUserGesture_0_5};

const PPB_FileChooserTrusted_0_6 g_ppb_filechoosertrusted_thunk_0_6 = {
    &ShowWithoutUserGesture};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_FileChooserTrusted_0_5*
GetPPB_FileChooserTrusted_0_5_Thunk() {
  return &g_ppb_filechoosertrusted_thunk_0_5;
}

PPAPI_THUNK_EXPORT const PPB_FileChooserTrusted_0_6*
GetPPB_FileChooserTrusted_0_6_Thunk() {
  return &g_ppb_filechoosertrusted_thunk_0_6;
}

}  // namespace thunk
}  // namespace ppapi
