// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_X_UTIL_H_
#define GPU_IPC_SERVICE_X_UTIL_H_

// Some X-Windows specific stuff. This can be included on any platform, and will
// be a NOP on non-Linux ones.

#include "build/build_config.h"
#include "gpu/ipc/service/gpu_config.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

namespace gpu {

// Forward declares ------------------------------------------------------------
//
// X Windows headers do a lot of evil stuff, like "#define Status int" which
// will cause many problems when combined with our other header files (like
// ones that define a class local enum called "Status."
//
// These definitions are not Kosher, but allow us to remove this dependency and
// actually compile X at all.

typedef unsigned long XID;

extern "C" {

typedef struct _XDisplay Display;
typedef struct __GLXcontextRec *GLXContext;

}  // extern "C"

}  // namespace gpu

#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(OZONE_PLATFORM_X11)

#endif  // GPU_IPC_SERVICE_X_UTIL_H_
