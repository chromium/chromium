// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_PATHS_H_
#define MEDIA_CDM_CDM_PATHS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/token.h"

namespace media {

// Name of the ClearKey CDM library.
extern const char kClearKeyCdmLibraryName[];

extern const char kClearKeyCdmBaseDirectory[];

// Display name for Clear Key CDM.
extern const char kClearKeyCdmDisplayName[];

// The default GUID for Clear Key Cdm.
extern const base::Token kClearKeyCdmGuid;

// A different GUID for Clear Key Cdm for testing running different types of
// CDMs in the system.
extern const base::Token kClearKeyCdmDifferentGuid;

// Identifier used by the PluginPrivateFileSystem to identify the files stored
// for the Clear Key CDM.
extern const char kClearKeyCdmFileSystemId[];

// Returns the path of a CDM relative to DIR_COMPONENTS.
// On platforms where a platform specific path is used, returns
//   |cdm_base_path|/_platform_specific/<platform>_<arch>
//   e.g. WidevineCdm/_platform_specific/win_x64
// Otherwise, returns an empty path.
// TODO(xhwang): Use this function in Widevine CDM component installer.
base::FilePath GetPlatformSpecificDirectory(
    const base::FilePath& cdm_base_path);
base::FilePath GetPlatformSpecificDirectory(const std::string& cdm_base_path);

}  // namespace media

#endif  // MEDIA_CDM_CDM_PATHS_H_
