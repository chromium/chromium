// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/branding.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include <unistd.h>

#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/username.h"
#endif

namespace {

// TODO(lambroslambrou): The default locations should depend on whether Chrome
// branding is enabled - this means also modifying the Python daemon script.
// The actual location of the files is ultimately determined by the service
// daemon and native messaging host - these defaults are only used in case the
// command-line switches are absent.
#if BUILDFLAG(IS_WIN)
#ifdef OFFICIAL_BUILD
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("Google\\Chrome Remote Desktop");
#else
const base::FilePath::CharType kConfigDir[] = FILE_PATH_LITERAL("Chromoting");
#endif
#elif BUILDFLAG(IS_APPLE)
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("Chrome Remote Desktop");
#elif !BUILDFLAG(IS_LINUX)
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL(".config/chrome-remote-desktop");
#endif

#if !BUILDFLAG(IS_LINUX)
base::FilePath GetConfigDirWithPrefix(int prefix_path_key) {
  base::FilePath app_data_dir;
  base::PathService::Get(prefix_path_key, &app_data_dir);
  if (app_data_dir.empty()) {
    LOG(ERROR) << "Failed to get path for key: " << prefix_path_key;
    return {};
  }
  return app_data_dir.Append(kConfigDir);
}
#endif

}  // namespace

namespace remoting {

#if BUILDFLAG(IS_WIN)
const wchar_t kWindowsServiceName[] = L"chromoting";
#endif

base::FilePath GetConfigDir() {
#if BUILDFLAG(IS_LINUX)
  if (getuid() == /*root*/ 0 || GetUsername() == GetNetworkProcessUsername()) {
    // Processes run as root:
    //     daemon process,
    //     elevated native messaging host (for managing multi-process host)
    // Processes run as network user: network process
    return GetMultiProcessHostGlobalConfigDir();
  } else {
    // Other processes:
    //     single-process host,
    //     desktop process,
    //     user launched processes (e.g. remoting-webauthn),
    //     unelevated native messaging host (for managing single-process host)
    return GetPerUserConfigDir();
  }
#elif BUILDFLAG(IS_WIN)
  return GetConfigDirWithPrefix(base::DIR_COMMON_APP_DATA);
#elif BUILDFLAG(IS_APPLE)
  return GetConfigDirWithPrefix(base::DIR_APP_DATA);
#else
  return GetConfigDirWithPrefix(base::DIR_HOME);
#endif
}

}  // namespace remoting
