/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_PPAPI_SIMPLE_PS_INSTANCE_H_
#define LIBRARIES_PPAPI_SIMPLE_PS_INSTANCE_H_

#include <stdarg.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_var.h"

#include "ppapi_simple/ps_event.h"

EXTERN_C_BEGIN

enum PSVerbosity {
  PSV_SILENT = 0,
  PSV_ERROR = 1,
  PSV_WARN = 2,
  PSV_LOG = 3,
  PSV_TRACE = 4,
};

void PSInstanceSetVerbosity(enum PSVerbosity verbosity);
void PSInstanceTrace(const char *fmt, ...);
void PSInstanceLog(const char *fmt, ...);
void PSInstanceWarn(const char *fmt, ...);
void PSInstanceError(const char *fmt, ...);

EXTERN_C_END

#endif  // LIBRARIES_PPAPI_SIMPLE_PS_INSTANCE_H_
