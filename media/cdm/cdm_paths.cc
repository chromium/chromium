// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_paths.h"

#include <string>

#include "build/build_config.h"
#include "media/cdm/cdm_type.h"
#include "media/media_buildflags.h"

namespace media {

// Name of the ClearKey CDM library.
const char kClearKeyCdmLibraryName[] = "clearkeycdm";

const char kClearKeyCdmBaseDirectory[] =
#if BUILDFLAG(IS_FUCHSIA)
    "lib/"
#endif
    "ClearKeyCdm";

const char kClearKeyCdmDisplayName[] = "Clear Key CDM";

const CdmType kClearKeyCdmType{0x3a2e0fadde4bd1b7ull, 0xcb90df3e240d1694ull};
const CdmType kClearKeyCdmDifferentCdmType{0xc3914773474bdb02ull,
                                           0x8e8de4d84d3ca030ull};

base::FilePath GetPlatformSpecificDirectory(
    const base::FilePath& cdm_base_path) {
  // CDM_PLATFORM_SPECIFIC_PATH is specified in cdm_paths.gni.
  const std::string kPlatformSpecific = BUILDFLAG(CDM_PLATFORM_SPECIFIC_PATH);
  if (kPlatformSpecific.empty())
    return base::FilePath();

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
