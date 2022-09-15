// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_graphics_3d.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetAttribMaxValue(PP_Resource instance,
                          int32_t attribute,
                          int32_t* value) {
  VLOG(4) << "PPB_Graphics3D::GetAttribMaxValue()";
  EnterResource<PPB_Graphics3D_API> enter(instance, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetAttribMaxValue(attribute, value);
}

PP_Resource Create(PP_Instance instance,
                   PP_Resource share_context,
                   const int32_t attrib_list[]) {
  VLOG(4) << "PPB_Graphics3D::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics3D(instance, share_context,
                                             attrib_list);
}

PP_Bool IsGraphics3D(PP_Resource resource) {
  VLOG(4) << "PPB_Graphics3D::IsGraphics3D()";
  EnterResource<PPB_Graphics3D_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetAttribs(PP_Resource context, int32_t attrib_list[]) {
  VLOG(4) << "PPB_Graphics3D::GetAttribs()";
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetAttribs(attrib_list);
}

int32_t SetAttribs(PP_Resource context, const int32_t attrib_list[]) {
  VLOG(4) << "PPB_Graphics3D::SetAttribs()";
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->SetAttribs(attrib_list);
}

int32_t GetError(PP_Resource context) {
  VLOG(4) << "PPB_Graphics3D::GetError()";
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetError();
}

int32_t ResizeBuffers(PP_Resource context, int32_t width, int32_t height) {
  VLOG(4) << "PPB_Graphics3D::ResizeBuffers()";
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->ResizeBuffers(width, height);
}

int32_t SwapBuffers(PP_Resource context,
                    struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Graphics3D::SwapBuffers()";
  EnterResource<PPB_Graphics3D_API> enter(context, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SwapBuffers(enter.callback()));
}

const PPB_Graphics3D_1_0 g_ppb_graphics3d_thunk_1_0 = {
    &GetAttribMaxValue, &Create,   &IsGraphics3D,  &GetAttribs,
    &SetAttribs,        &GetError, &ResizeBuffers, &SwapBuffers};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Graphics3D_1_0* GetPPB_Graphics3D_1_0_Thunk() {
  return &g_ppb_graphics3d_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
