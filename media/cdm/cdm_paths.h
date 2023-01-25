// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_PATHS_H_
#define MEDIA_CDM_CDM_PATHS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"

#if !BUILDFLAG(ENABLE_LIBRARY_CDMS)
#error This file only applies to builds that enable_library_cdms.
#endif

namespace media {

// Returns the path of a CDM relative to DIR_COMPONENTS.
// On platforms where a platform specific path is used, returns
//   |cdm_base_path|/_platform_specific/<platform>_<arch>
//   e.g. WidevineCdm/_platform_specific/win_x64
// Otherwise, returns an empty path.
// TODO(xhwang): Use this function in Widevine CDM component installer.
base::FilePath GetPlatformSpecificDirectory(
    const base::FilePath& cdm_base_path);
base::FilePath GetPlatformSpecificDirectory(const std::string& cdm_base_path);

#if BUILDFLAG(IS_WIN)
// Returns the "CDM store path" to be passed to `MediaFoundationCdm`. The
// `cdm_store_path_root` is typically the path to the Chrome user's profile,
// e.g.
// C:\Users\<user>\AppData\Local\Google\Chrome\Default\MediaFoundationCdmStore\x86_x64
base::FilePath GetCdmStorePath(const base::FilePath& cdm_store_path_root,
                               const base::UnguessableToken& cdm_origin_id,
                               const std::string& key_system);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace media

#endif  // MEDIA_CDM_CDM_PATHS_H_
