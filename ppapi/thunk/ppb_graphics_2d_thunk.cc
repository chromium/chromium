// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_graphics_2d.idl modified Fri Apr 15 15:37:20 2016.

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   const struct PP_Size* size,
                   PP_Bool is_always_opaque) {
  VLOG(4) << "PPB_Graphics2D::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics2D(instance, size, is_always_opaque);
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  VLOG(4) << "PPB_Graphics2D::IsGraphics2D()";
  EnterResource<PPB_Graphics2D_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool Describe(PP_Resource graphics_2d,
                 struct PP_Size* size,
                 PP_Bool* is_always_opaque) {
  VLOG(4) << "PPB_Graphics2D::Describe()";
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed()) {
    memset(size, 0, sizeof(*size));
    memset(is_always_opaque, 0, sizeof(*is_always_opaque));
    return PP_FALSE;
  }
  return enter.object()->Describe(size, is_always_opaque);
}

void PaintImageData(PP_Resource graphics_2d,
                    PP_Resource image_data,
                    const struct PP_Point* top_left,
                    const struct PP_Rect* src_rect) {
  VLOG(4) << "PPB_Graphics2D::PaintImageData()";
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->PaintImageData(image_data, top_left, src_rect);
}

void Scroll(PP_Resource graphics_2d,
            const struct PP_Rect* clip_rect,
            const struct PP_Point* amount) {
  VLOG(4) << "PPB_Graphics2D::Scroll()";
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->Scroll(clip_rect, amount);
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  VLOG(4) << "PPB_Graphics2D::ReplaceContents()";
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->ReplaceContents(image_data);
}

int32_t Flush(PP_Resource graphics_2d, struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Graphics2D::Flush()";
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Flush(enter.callback()));
}

PP_Bool SetScale(PP_Resource resource, float scale) {
  VLOG(4) << "PPB_Graphics2D::SetScale()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->SetScale(scale);
}

float GetScale(PP_Resource resource) {
  VLOG(4) << "PPB_Graphics2D::GetScale()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetScale();
}

PP_Bool SetLayerTransform(PP_Resource resource,
                          float scale,
                          const struct PP_Point* origin,
                          const struct PP_Point* translate) {
  VLOG(4) << "PPB_Graphics2D::SetLayerTransform()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->SetLayerTransform(scale, origin, translate);
}

const PPB_Graphics2D_1_0 g_ppb_graphics2d_thunk_1_0 = {
    &Create, &IsGraphics2D,    &Describe, &PaintImageData,
    &Scroll, &ReplaceContents, &Flush};

const PPB_Graphics2D_1_1 g_ppb_graphics2d_thunk_1_1 = {
    &Create,          &IsGraphics2D, &Describe, &PaintImageData, &Scroll,
    &ReplaceContents, &Flush,        &SetScale, &GetScale};

const PPB_Graphics2D_1_2 g_ppb_graphics2d_thunk_1_2 = {
    &Create,   &IsGraphics2D,     &Describe, &PaintImageData,
    &Scroll,   &ReplaceContents,  &Flush,    &SetScale,
    &GetScale, &SetLayerTransform};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Graphics2D_1_0* GetPPB_Graphics2D_1_0_Thunk() {
  return &g_ppb_graphics2d_thunk_1_0;
}

PPAPI_THUNK_EXPORT const PPB_Graphics2D_1_1* GetPPB_Graphics2D_1_1_Thunk() {
  return &g_ppb_graphics2d_thunk_1_1;
}

PPAPI_THUNK_EXPORT const PPB_Graphics2D_1_2* GetPPB_Graphics2D_1_2_Thunk() {
  return &g_ppb_graphics2d_thunk_1_2;
}

}  // namespace thunk
}  // namespace ppapi
