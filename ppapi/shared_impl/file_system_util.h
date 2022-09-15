// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_SYSTEM_UTIL_H_
#define PPAPI_SHARED_IMPL_FILE_SYSTEM_UTIL_H_

#include <string>

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

PPAPI_SHARED_EXPORT bool FileSystemTypeIsValid(PP_FileSystemType type);

PPAPI_SHARED_EXPORT bool FileSystemTypeHasQuota(PP_FileSystemType type);

PPAPI_SHARED_EXPORT std::string IsolatedFileSystemTypeToRootName(
    PP_IsolatedFileSystemType_Private type);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_SYSTEM_UTIL_H_
