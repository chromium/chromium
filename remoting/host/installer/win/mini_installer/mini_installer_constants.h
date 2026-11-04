// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_MINI_INSTALLER_MINI_INSTALLER_CONSTANTS_H_
#define REMOTING_HOST_INSTALLER_WIN_MINI_INSTALLER_MINI_INSTALLER_CONSTANTS_H_

namespace remoting::installer {

// Name of the setup executable unpacked from the embedded archive.
inline constexpr wchar_t kSetupBinaryName[] = L"remoting_setup.exe";

// PE resource name and type for the LZMA/7z payload archive embedded in
// `remoting_mini_installer.exe`. Must match `_ARCHIVE_RESOURCE_NAME` and
// `_ARCHIVE_RESOURCE_TYPE` in `create_remoting_installer_archive.py`.
inline constexpr wchar_t kArchiveResourceName[] = L"REMOTING.7Z";
inline constexpr wchar_t kArchiveResourceType[] = L"B7";

// Exit codes returned directly by `remoting_mini_installer.exe` when bootstrap
// extraction or process launch fails prior to `remoting_setup.exe` returning an
// exit code. Values start at 100 to avoid colliding with `SetupExitCode`.
enum class MiniInstallerExitCode : int {
  kResourceNotFound = 100,
  kTempDirCreationFailed = 101,
  kUnpackArchiveFailed = 102,
  kSetupBinaryNotFound = 103,
  kSetupLaunchFailed = 104,
  kSetupWaitFailed = 105,
};

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_MINI_INSTALLER_MINI_INSTALLER_CONSTANTS_H_
