// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_REF_UTIL_H_
#define PPAPI_SHARED_IMPL_FILE_REF_UTIL_H_

#include <string>

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class FilePath;
}

namespace ppapi {

// Routines to generate display names for internal and external file paths.
PPAPI_SHARED_EXPORT std::string GetNameForInternalFilePath(
    const std::string& path);
PPAPI_SHARED_EXPORT std::string GetNameForExternalFilePath(
    const base::FilePath& path);

// Determines whether an internal file path is valid.
PPAPI_SHARED_EXPORT bool IsValidInternalPath(const std::string& path);

// Determines whether an external file path is valid.
PPAPI_SHARED_EXPORT bool IsValidExternalPath(const base::FilePath& path);

// If path ends with a slash, normalize it away unless it's the root path.
PPAPI_SHARED_EXPORT void NormalizeInternalPath(std::string* path);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_REF_UTIL_H_
