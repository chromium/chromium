// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_display_color_profile_private.idl modified Wed Jan 27
// 17:10:16 2016.

#include <stdint.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_display_color_profile_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_display_color_profile_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_DisplayColorProfile_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateDisplayColorProfilePrivate(instance);
}

PP_Bool IsDisplayColorProfile(PP_Resource resource) {
  VLOG(4) << "PPB_DisplayColorProfile_Private::IsDisplayColorProfile()";
  EnterResource<PPB_DisplayColorProfile_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetColorProfile(PP_Resource display_color_profile_res,
                        struct PP_ArrayOutput color_profile,
                        struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_DisplayColorProfile_Private::GetColorProfile()";
  EnterResource<PPB_DisplayColorProfile_API> enter(display_color_profile_res,
                                                   callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetColorProfile(color_profile, enter.callback()));
}

int32_t RegisterColorProfileChangeCallback(
    PP_Resource display_color_profile_res,
    struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_DisplayColorProfile_Private::"
             "RegisterColorProfileChangeCallback()";
  EnterResource<PPB_DisplayColorProfile_API> enter(display_color_profile_res,
                                                   callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->RegisterColorProfileChangeCallback(enter.callback()));
}

const PPB_DisplayColorProfile_Private_0_1
    g_ppb_displaycolorprofile_private_thunk_0_1 = {
        &Create, &IsDisplayColorProfile, &GetColorProfile,
        &RegisterColorProfileChangeCallback};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_DisplayColorProfile_Private_0_1*
GetPPB_DisplayColorProfile_Private_0_1_Thunk() {
  return &g_ppb_displaycolorprofile_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
