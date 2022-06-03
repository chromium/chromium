// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_uma_private.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_uma_singleton_api.h"

namespace ppapi {
namespace thunk {

namespace {

void HistogramCustomTimes(PP_Instance instance,
                          struct PP_Var name,
                          int64_t sample,
                          int64_t min,
                          int64_t max,
                          uint32_t bucket_count) {
  VLOG(4) << "PPB_UMA_Private::HistogramCustomTimes()";
  EnterInstanceAPI<PPB_UMA_Singleton_API> enter(instance);
  if (enter.failed())
    return;
  enter.functions()->HistogramCustomTimes(instance, name, sample, min, max,
                                          bucket_count);
}

void HistogramCustomCounts(PP_Instance instance,
                           struct PP_Var name,
                           int32_t sample,
                           int32_t min,
                           int32_t max,
                           uint32_t bucket_count) {
  VLOG(4) << "PPB_UMA_Private::HistogramCustomCounts()";
  EnterInstanceAPI<PPB_UMA_Singleton_API> enter(instance);
  if (enter.failed())
    return;
  enter.functions()->HistogramCustomCounts(instance, name, sample, min, max,
                                           bucket_count);
}

void HistogramEnumeration(PP_Instance instance,
                          struct PP_Var name,
                          int32_t sample,
                          int32_t boundary_value) {
  VLOG(4) << "PPB_UMA_Private::HistogramEnumeration()";
  EnterInstanceAPI<PPB_UMA_Singleton_API> enter(instance);
  if (enter.failed())
    return;
  enter.functions()->HistogramEnumeration(instance, name, sample,
                                          boundary_value);
}

int32_t IsCrashReportingEnabled(PP_Instance instance,
                                struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_UMA_Private::IsCrashReportingEnabled()";
  EnterInstanceAPI<PPB_UMA_Singleton_API> enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.functions()->IsCrashReportingEnabled(instance, enter.callback()));
}

const PPB_UMA_Private_0_3 g_ppb_uma_private_thunk_0_3 = {
    &HistogramCustomTimes, &HistogramCustomCounts, &HistogramEnumeration,
    &IsCrashReportingEnabled};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_UMA_Private_0_3* GetPPB_UMA_Private_0_3_Thunk() {
  return &g_ppb_uma_private_thunk_0_3;
}

}  // namespace thunk
}  // namespace ppapi
