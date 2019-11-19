// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_paths.h"

#include <string>

#include "media/media_buildflags.h"

namespace media {

// Name of the ClearKey CDM library.
const char kClearKeyCdmLibraryName[] = "clearkeycdm";

const char kClearKeyCdmBaseDirectory[] = "ClearKeyCdm";
const char kClearKeyCdmDisplayName[] = "Clear Key CDM";
const base::Token kClearKeyCdmGuid{0x3a2e0fadde4bd1b7ull,
                                   0xcb90df3e240d1694ull};
const base::Token kClearKeyCdmDifferentGuid{0xc3914773474bdb02ull,
                                            0x8e8de4d84d3ca030ull};

// As the file system was initially used by the CDM running as a pepper plugin,
// this ID is based on the pepper plugin MIME type.
const char kClearKeyCdmFileSystemId[] = "application_x-ppapi-clearkey-cdm";

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

}  // namespace media
