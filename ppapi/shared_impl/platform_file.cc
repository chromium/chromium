// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ppapi/shared_impl/platform_file.h"

namespace ppapi {

// TODO(piman/brettw): Change trusted interface to return a PP_FileHandle,
// those casts are ugly.
base::PlatformFile IntToPlatformFile(int32_t handle) {
#if BUILDFLAG(IS_WIN)
  return reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
#elif BUILDFLAG(IS_POSIX)
  return handle;
#else
#error Not implemented.
#endif
}

int32_t PlatformFileToInt(base::PlatformFile handle) {
#if BUILDFLAG(IS_WIN)
  return static_cast<int32_t>(reinterpret_cast<intptr_t>(handle));
#elif BUILDFLAG(IS_POSIX)
  return handle;
#else
#error Not implemented.
#endif
}

}  // namespace ppapi
