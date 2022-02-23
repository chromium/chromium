// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_constants.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/mojo_ipc/mojo_ipc_util.h"

namespace remoting {

namespace {

#if !defined(NDEBUG) && BUILDFLAG(IS_LINUX)
// Use a different IPC name for Linux debug builds so that we can run the host
// directly from out/Debug without interfering with the production host that
// might also be running.
constexpr char kChromotingHostServicesIpcName[] =
    "chromoting_host_services_debug_mojo_ipc";
#else
constexpr char kChromotingHostServicesIpcName[] =
    "chromoting_host_services_mojo_ipc";
#endif

}  // namespace

const base::FilePath::CharType kHostBinaryName[] =
    FILE_PATH_LITERAL("remoting_host");

const base::FilePath::CharType kDesktopBinaryName[] =
    FILE_PATH_LITERAL("remoting_desktop");

bool GetInstalledBinaryPath(const base::FilePath::StringType& binary,
                            base::FilePath* full_path) {
  base::FilePath dir_path;
  if (!base::PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    return false;
  }

  base::FilePath path = dir_path.Append(binary);

#if BUILDFLAG(IS_WIN)
  path = path.ReplaceExtension(FILE_PATH_LITERAL("exe"));
#endif  // BUILDFLAG(IS_WIN)

  *full_path = path;
  return true;
}

const mojo::NamedPlatformChannel::ServerName&
GetChromotingHostServicesServerName() {
  static const base::NoDestructor<mojo::NamedPlatformChannel::ServerName>
      server_name(WorkingDirectoryIndependentServerNameFromUTF8(
          kChromotingHostServicesIpcName));
  return *server_name;
}

}  // namespace remoting
