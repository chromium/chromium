// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_find_private.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_find_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

void SetPluginToHandleFindRequests(PP_Instance instance) {
  VLOG(4) << "PPB_Find_Private::SetPluginToHandleFindRequests()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SetPluginToHandleFindRequests(instance);
}

void NumberOfFindResultsChanged(PP_Instance instance,
                                int32_t total,
                                PP_Bool final_result) {
  VLOG(4) << "PPB_Find_Private::NumberOfFindResultsChanged()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->NumberOfFindResultsChanged(instance, total, final_result);
}

void SelectedFindResultChanged(PP_Instance instance, int32_t index) {
  VLOG(4) << "PPB_Find_Private::SelectedFindResultChanged()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SelectedFindResultChanged(instance, index);
}

void SetTickmarks(PP_Instance instance,
                  const struct PP_Rect tickmarks[],
                  uint32_t count) {
  VLOG(4) << "PPB_Find_Private::SetTickmarks()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SetTickmarks(instance, tickmarks, count);
}

const PPB_Find_Private_0_3 g_ppb_find_private_thunk_0_3 = {
    &SetPluginToHandleFindRequests, &NumberOfFindResultsChanged,
    &SelectedFindResultChanged, &SetTickmarks};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Find_Private_0_3* GetPPB_Find_Private_0_3_Thunk() {
  return &g_ppb_find_private_thunk_0_3;
}

}  // namespace thunk
}  // namespace ppapi
