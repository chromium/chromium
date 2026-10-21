// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_INSTALLER_CONSTANTS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_INSTALLER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "remoting/host/installer/win/setup/payload_files.h"

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

// Branding-dependent product strings and registry paths.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr wchar_t kServiceDisplayName[] =
    L"Chrome Remote Desktop Service";
inline constexpr wchar_t kServiceDescription[] =
    L"This service enables incoming Chrome Remote Desktop connections.";
inline constexpr wchar_t kProductName[] = L"Chrome Remote Desktop Host";
inline constexpr wchar_t kManufacturer[] = L"Google LLC";
inline constexpr wchar_t kChromotingKeyPath[] =
    L"Google\\Chrome Remote Desktop";
inline constexpr wchar_t kUrlForwarderName[] =
    L"Chrome Remote Desktop URL Forwarder";
inline constexpr wchar_t kUrlForwarderProgId[] =
    L"ChromeRemoteDesktopUrlForwarder";
#else
inline constexpr wchar_t kServiceDisplayName[] = L"Chromoting Service";
inline constexpr wchar_t kServiceDescription[] =
    L"This service enables incoming Chromoting connections.";
inline constexpr wchar_t kProductName[] = L"Chromoting Host";
inline constexpr wchar_t kManufacturer[] = L"The Chromium Authors";
inline constexpr wchar_t kChromotingKeyPath[] = L"Chromoting";
inline constexpr wchar_t kUrlForwarderName[] = L"Chromoting URL Forwarder";
inline constexpr wchar_t kUrlForwarderProgId[] = L"ChromotingUrlForwarder";
#endif

// Executable and library names.
inline constexpr wchar_t kHostBinaryName[] = L"remoting_host.exe";
inline constexpr wchar_t kCoreBinaryName[] = L"remoting_core.dll";
inline constexpr wchar_t kRemoteSecurityKeyBinaryName[] =
    L"remote_security_key.exe";
inline constexpr wchar_t kRemoteOpenUrlBinaryName[] = L"remote_open_url.exe";

// Windows EventLog source name.
inline constexpr wchar_t kEventSourceName[] = L"chromoting";

// COM AppID, Interface IIDs, and TypeLib GUIDs for RdpDesktopSession.
inline constexpr wchar_t kRdpAppId[] =
    L"{52e6fd1a-f16e-49c0-aacb-5436a915448b}";
inline constexpr wchar_t kChromotingTypeLibId[] =
    L"{b6396c45-b0cc-456b-9f49-f12964ee6df4}";
inline constexpr wchar_t kIRdpDesktopSessionId[] =
    L"{6a7699f0-ee43-43e7-aa30-a6738f9bd470}";
inline constexpr wchar_t kIRdpDesktopSessionEventHandlerId[] =
    L"{b59b96da-83cb-40ee-9b91-c377400fc3e3}";
inline constexpr wchar_t kIDaemonControlId[] =
    L"{e051a481-6345-4ba1-bdb1-cf7929955268}";
inline constexpr wchar_t kIDaemonControl2Id[] =
    L"{655bd819-c08c-4b04-80c2-f160739ff6ef}";
inline constexpr wchar_t kUniversalMarshalerClsid[] =
    L"{00020424-0000-0000-C000-000000000046}";

// Extension appended to locked in-use binaries during an update so the new
// binary can be copied into the canonical path.
inline constexpr wchar_t kOldFileExtension[] = L".old";

// Returns the target installation directory for Chrome Remote Desktop
// (e.g. C:\Program Files\Google\Chrome Remote Desktop).
base::FilePath GetDefaultInstallDir();

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_INSTALLER_CONSTANTS_H_
