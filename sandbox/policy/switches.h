// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SWITCHES_H_
#define SANDBOX_POLICY_SWITCHES_H_

#include "build/build_config.h"
#include "sandbox/policy/export.h"

namespace sandbox::policy::switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
SANDBOX_POLICY_EXPORT extern const char kServiceSandboxType[];

// Flags owned by the service manager sandbox.
SANDBOX_POLICY_EXPORT extern const char kAllowSandboxDebugging[];
SANDBOX_POLICY_EXPORT extern const char kDisableGpuSandbox[];
SANDBOX_POLICY_EXPORT extern const char kDisableNamespaceSandbox[];
SANDBOX_POLICY_EXPORT extern const char kDisableSeccompFilterSandbox[];
SANDBOX_POLICY_EXPORT extern const char kDisableSetuidSandbox[];
SANDBOX_POLICY_EXPORT extern const char kGpuSandboxAllowSysVShm[];
SANDBOX_POLICY_EXPORT extern const char kGpuSandboxFailuresFatal[];
SANDBOX_POLICY_EXPORT extern const char kNoSandbox[];
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
SANDBOX_POLICY_EXPORT extern const char kNoZygoteSandbox[];
#endif
#if BUILDFLAG(IS_WIN)
SANDBOX_POLICY_EXPORT extern const char kAllowThirdPartyModules[];
SANDBOX_POLICY_EXPORT extern const char kAddXrAppContainerCaps[];
#endif
#if BUILDFLAG(IS_MAC)
SANDBOX_POLICY_EXPORT extern const char kEnableSandboxLogging[];
SANDBOX_POLICY_EXPORT extern const char kDisableMetalShaderCache[];
#endif

// Flags spied upon from other layers.
SANDBOX_POLICY_EXPORT extern const char kProcessType[];
SANDBOX_POLICY_EXPORT extern const char kGpuProcess[];
SANDBOX_POLICY_EXPORT extern const char kNaClLoaderProcess[];
SANDBOX_POLICY_EXPORT extern const char kPpapiPluginProcess[];
SANDBOX_POLICY_EXPORT extern const char kRendererProcess[];
SANDBOX_POLICY_EXPORT extern const char kUtilityProcess[];
SANDBOX_POLICY_EXPORT extern const char kZygoteProcessType[];
SANDBOX_POLICY_EXPORT extern const char kRelauncherProcessType[];
SANDBOX_POLICY_EXPORT extern const char kCodeSignCloneCleanupProcessType[];

}  // namespace sandbox::policy::switches

#endif  // SANDBOX_POLICY_SWITCHES_H_
