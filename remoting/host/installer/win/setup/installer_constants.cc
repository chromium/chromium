// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/installer_constants.h"

#include "base/base_paths_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"

namespace remoting::installer {

base::FilePath GetDefaultInstallDir() {
  base::FilePath program_files_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES6432,
                              &program_files_dir)) {
    LOG(ERROR) << "Failed to resolve DIR_PROGRAM_FILES6432";
    return {};
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return program_files_dir.Append(FILE_PATH_LITERAL("Google"))
      .Append(FILE_PATH_LITERAL("Chrome Remote Desktop"));
#else
  return program_files_dir.Append(FILE_PATH_LITERAL("Chromoting"));
#endif
}

base::FilePath GetDefaultHostConfigPath() {
  base::FilePath common_app_data_dir;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA,
                              &common_app_data_dir)) {
    LOG(ERROR) << "Failed to resolve DIR_COMMON_APP_DATA";
    return {};
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return common_app_data_dir.Append(FILE_PATH_LITERAL("Google"))
      .Append(FILE_PATH_LITERAL("Chrome Remote Desktop"))
      .Append(kHostConfigFileName);
#else
  return common_app_data_dir.Append(FILE_PATH_LITERAL("Chromoting"))
      .Append(kHostConfigFileName);
#endif
}

}  // namespace remoting::installer
