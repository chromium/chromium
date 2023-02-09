// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_paths.h"

#include <string>

#include "build/build_config.h"
#include "media/media_buildflags.h"

namespace media {

base::FilePath GetPlatformSpecificDirectory(
    const base::FilePath& cdm_base_path) {
  // CDM_PLATFORM_SPECIFIC_PATH is specified in cdm_paths.gni.
  const std::string kPlatformSpecific = BUILDFLAG(CDM_PLATFORM_SPECIFIC_PATH);
  if (kPlatformSpecific.empty()) {
    return base::FilePath();
  }

  return cdm_base_path.AppendASCII(kPlatformSpecific).NormalizePathSeparators();
}

base::FilePath GetPlatformSpecificDirectory(const std::string& cdm_base_path) {
  return GetPlatformSpecificDirectory(
      base::FilePath::FromUTF8Unsafe(cdm_base_path));
}

#if BUILDFLAG(IS_WIN)
base::FilePath GetCdmStorePath(const base::FilePath& cdm_store_path_root,
                               const base::UnguessableToken& cdm_origin_id,
                               const std::string& key_system) {
  return cdm_store_path_root.AppendASCII(cdm_origin_id.ToString())
      .AppendASCII(key_system);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace media
