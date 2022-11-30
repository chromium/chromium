// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_console.idl modified Wed Jan 27 17:10:16 2016.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

void Log(PP_Instance instance, PP_LogLevel level, struct PP_Var value) {
  VLOG(4) << "PPB_Console::Log()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->Log(instance, level, value);
}

void LogWithSource(PP_Instance instance,
                   PP_LogLevel level,
                   struct PP_Var source,
                   struct PP_Var value) {
  VLOG(4) << "PPB_Console::LogWithSource()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->LogWithSource(instance, level, source, value);
}

const PPB_Console_1_0 g_ppb_console_thunk_1_0 = {&Log, &LogWithSource};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Console_1_0* GetPPB_Console_1_0_Thunk() {
  return &g_ppb_console_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
