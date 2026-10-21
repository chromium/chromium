// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_UTIL_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_UTIL_H_

#include "base/files/file_path.h"

namespace remoting::installer {

// Registers all host registry keys under HKLM for a flat installation in
// `install_dir`:
// - Native Messaging host manifests (Chrome and Firefox, 32-bit & 64-bit views)
// - Windows EventLog source (`chromoting`)
// - Remote Security Key binary path
// - URL Forwarder ProgID, Capabilities, and RegisteredApplications
// - COM server, proxy/stub, and TypeLib entries for `RdpDesktopSession`
// - Pairing (`clients` and ACL-protected `secrets`) and logging registry keys
// - Omaha Clients registration (on branded builds)
bool RegisterHostRegistryEntries(const base::FilePath& install_dir);

// Removes all host registry keys registered by RegisterHostRegistryEntries().
void UnregisterHostRegistryEntries();

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_UTIL_H_
