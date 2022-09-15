// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_TYPE_CONVERSION_H_
#define PPAPI_SHARED_IMPL_FILE_TYPE_CONVERSION_H_

#include "base/files/file.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

PPAPI_SHARED_EXPORT int FileErrorToPepperError(base::File::Error error_code);

// Converts a PP_FileOpenFlags_Dev flag combination into a corresponding
// PlatformFileFlags flag combination.
// Returns |true| if okay.
PPAPI_SHARED_EXPORT bool PepperFileOpenFlagsToPlatformFileFlags(
    int32_t pp_open_flags,
    uint32_t* flags_out);

PPAPI_SHARED_EXPORT void FileInfoToPepperFileInfo(const base::File::Info& info,
                                                  PP_FileSystemType fs_type,
                                                  PP_FileInfo* info_out);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_TYPE_CONVERSION_H_
