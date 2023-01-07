// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PLATFORM_FILE_H_
#define PPAPI_SHARED_IMPL_PLATFORM_FILE_H_

#include "base/files/file.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

PPAPI_SHARED_EXPORT base::PlatformFile IntToPlatformFile(int32_t handle);
PPAPI_SHARED_EXPORT int32_t PlatformFileToInt(base::PlatformFile handle);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PLATFORM_FILE_H_
