// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_INSTALLER_CONSTANTS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_INSTALLER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "build/branding_buildflags.h"

namespace remoting::installer {

// Command-line switches for remoting_setup.exe.
inline constexpr char kInstallSwitch[] = "install";
inline constexpr char kUninstallSwitch[] = "uninstall";
inline constexpr char kUpdateSwitch[] = "update";

// Omaha App ID for Chrome Remote Desktop Host.
inline constexpr wchar_t kOmahaAppId[] =
    L"{b210701e-ffc4-49e3-932b-370728c72662}";

// Legacy WiX MSI UpgradeCode used to detect and migrate existing 32-bit MSI
// installations.
inline constexpr wchar_t kLegacyMsiUpgradeCode[] =
    L"{2b21f767-e157-4fa6-963c-55834c1433a6}";

// Service display name and description.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr wchar_t kServiceDisplayName[] =
    L"Chrome Remote Desktop Service";
inline constexpr wchar_t kServiceDescription[] =
    L"This service enables incoming Chrome Remote Desktop connections.";
#else
inline constexpr wchar_t kServiceDisplayName[] = L"Chromoting Service";
inline constexpr wchar_t kServiceDescription[] =
    L"This service enables incoming Chromoting connections.";
#endif

// Executable and library names.
inline constexpr wchar_t kHostBinaryName[] = L"remoting_host.exe";
inline constexpr wchar_t kCoreBinaryName[] = L"remoting_core.dll";

// Returns the target installation directory for Chrome Remote Desktop
// (e.g. C:\Program Files\Google\Chrome Remote Desktop).
base::FilePath GetDefaultInstallDir();

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_INSTALLER_CONSTANTS_H_
