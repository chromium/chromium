// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_file_chooser_dev.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_FileChooserMode_Dev mode,
                   struct PP_Var accept_types) {
  VLOG(4) << "PPB_FileChooser_Dev::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFileChooser(instance, mode, accept_types);
}

PP_Bool IsFileChooser(PP_Resource resource) {
  VLOG(4) << "PPB_FileChooser_Dev::IsFileChooser()";
  EnterResource<PPB_FileChooser_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Show_0_5(PP_Resource chooser, struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileChooser_Dev::Show_0_5()";
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Show0_5(enter.callback()));
}

PP_Resource GetNextChosenFile(PP_Resource chooser) {
  VLOG(4) << "PPB_FileChooser_Dev::GetNextChosenFile()";
  EnterResource<PPB_FileChooser_API> enter(chooser, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNextChosenFile();
}

int32_t Show(PP_Resource chooser,
             struct PP_ArrayOutput output,
             struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_FileChooser_Dev::Show()";
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Show(output, enter.callback()));
}

const PPB_FileChooser_Dev_0_5 g_ppb_filechooser_dev_thunk_0_5 = {
    &Create, &IsFileChooser, &Show_0_5, &GetNextChosenFile};

const PPB_FileChooser_Dev_0_6 g_ppb_filechooser_dev_thunk_0_6 = {
    &Create, &IsFileChooser, &Show};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_FileChooser_Dev_0_5*
GetPPB_FileChooser_Dev_0_5_Thunk() {
  return &g_ppb_filechooser_dev_thunk_0_5;
}

PPAPI_THUNK_EXPORT const PPB_FileChooser_Dev_0_6*
GetPPB_FileChooser_Dev_0_6_Thunk() {
  return &g_ppb_filechooser_dev_thunk_0_6;
}

}  // namespace thunk
}  // namespace ppapi
