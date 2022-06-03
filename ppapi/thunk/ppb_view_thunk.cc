// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_view.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_view_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsView(PP_Resource resource) {
  VLOG(4) << "PPB_View::IsView()";
  EnterResource<PPB_View_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool GetRect(PP_Resource resource, struct PP_Rect* rect) {
  VLOG(4) << "PPB_View::GetRect()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRect(rect);
}

PP_Bool IsFullscreen(PP_Resource resource) {
  VLOG(4) << "PPB_View::IsFullscreen()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsFullscreen();
}

PP_Bool IsVisible(PP_Resource resource) {
  VLOG(4) << "PPB_View::IsVisible()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsVisible();
}

PP_Bool IsPageVisible(PP_Resource resource) {
  VLOG(4) << "PPB_View::IsPageVisible()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsPageVisible();
}

PP_Bool GetClipRect(PP_Resource resource, struct PP_Rect* clip) {
  VLOG(4) << "PPB_View::GetClipRect()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetClipRect(clip);
}

float GetDeviceScale(PP_Resource resource) {
  VLOG(4) << "PPB_View::GetDeviceScale()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetDeviceScale();
}

float GetCSSScale(PP_Resource resource) {
  VLOG(4) << "PPB_View::GetCSSScale()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetCSSScale();
}

PP_Bool GetScrollOffset(PP_Resource resource, struct PP_Point* offset) {
  VLOG(4) << "PPB_View::GetScrollOffset()";
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetScrollOffset(offset);
}

const PPB_View_1_0 g_ppb_view_thunk_1_0 = {
    &IsView, &GetRect, &IsFullscreen, &IsVisible, &IsPageVisible, &GetClipRect};

const PPB_View_1_1 g_ppb_view_thunk_1_1 = {
    &IsView,        &GetRect,     &IsFullscreen,   &IsVisible,
    &IsPageVisible, &GetClipRect, &GetDeviceScale, &GetCSSScale};

const PPB_View_1_2 g_ppb_view_thunk_1_2 = {
    &IsView,         &GetRect,       &IsFullscreen,
    &IsVisible,      &IsPageVisible, &GetClipRect,
    &GetDeviceScale, &GetCSSScale,   &GetScrollOffset};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_View_1_0* GetPPB_View_1_0_Thunk() {
  return &g_ppb_view_thunk_1_0;
}

PPAPI_THUNK_EXPORT const PPB_View_1_1* GetPPB_View_1_1_Thunk() {
  return &g_ppb_view_thunk_1_1;
}

PPAPI_THUNK_EXPORT const PPB_View_1_2* GetPPB_View_1_2_Thunk() {
  return &g_ppb_view_thunk_1_2;
}

}  // namespace thunk
}  // namespace ppapi
