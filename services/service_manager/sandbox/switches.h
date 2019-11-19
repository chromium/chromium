// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_SWITCHES_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_SWITCHES_H_

#include "build/build_config.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/export.h"

namespace service_manager {
namespace switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kServiceSandboxType[];

// Must be in sync with "sandbox_type" values as used in service manager's
// manifest.json catalog files.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNoneSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char
    kNoneSandboxAndElevatedPrivileges[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNetworkSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPpapiSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kUtilitySandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kCdmSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kXrCompositingSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPdfCompositorSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kProfilingSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAudioSandbox[];
#if defined(OS_CHROMEOS)
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kImeSandbox[];
#endif  // OS_CHROMEOS

// Flags owned by the service manager sandbox.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAllowNoSandboxJob[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAllowSandboxDebugging[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableGpuSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableNamespaceSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableSeccompFilterSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableSetuidSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableWin32kLockDown[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kEnableAudioServiceSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kGpuSandboxAllowSysVShm[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kGpuSandboxFailuresFatal[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNoSandbox[];
#if defined(OS_WIN)
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAllowThirdPartyModules[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAddGpuAppContainerCaps[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableGpuAppContainer[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableGpuLpac[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kEnableGpuAppContainer[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char
    kNoSandboxAndElevatedPrivileges[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAddXrAppContainerCaps[];
#endif
#if defined(OS_MACOSX)
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kEnableSandboxLogging[];
#endif

// Flags spied upon from other layers.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kGpuProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNaClLoaderProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPpapiBrokerProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPpapiPluginProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kRendererProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kUtilityProcess[];

}  // namespace switches

#if defined(OS_WIN)
// Returns whether Win32k lockdown is enabled for child processes or not.
// Not really a switch, but uses one under the covers.
SERVICE_MANAGER_SANDBOX_EXPORT bool IsWin32kLockdownEnabled();
#endif

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_SWITCHES_H_
