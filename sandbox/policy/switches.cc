// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/switches.h"

#include "build/build_config.h"

namespace sandbox::policy::switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
const char kServiceSandboxType[] = "service-sandbox-type";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[] = "allow-sandbox-debugging";

// Disables the GPU process sandbox.
const char kDisableGpuSandbox[] = "disable-gpu-sandbox";

// Disables usage of the namespace sandbox.
const char kDisableNamespaceSandbox[] = "disable-namespace-sandbox";

// Disable the seccomp filter sandbox (seccomp-bpf) (Linux only).
const char kDisableSeccompFilterSandbox[] = "disable-seccomp-filter-sandbox";

// Disable the setuid sandbox (Linux only).
const char kDisableSetuidSandbox[] = "disable-setuid-sandbox";

// Allows shmat() system call in the GPU sandbox.
const char kGpuSandboxAllowSysVShm[] = "gpu-sandbox-allow-sysv-shm";

// Makes GPU sandbox failures fatal.
const char kGpuSandboxFailuresFatal[] = "gpu-sandbox-failures-fatal";

// Disables the sandbox for all process types that are normally sandboxed.
// Meant to be used as a browser-level switch for testing purposes only.
const char kNoSandbox[] = "no-sandbox";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Instructs the zygote to launch without a sandbox. Processes forked from this
// type of zygote will apply their own custom sandboxes later.
const char kNoZygoteSandbox[] = "no-zygote-sandbox";
#endif

#if BUILDFLAG(IS_WIN)
// Allows third party modules to inject by disabling the BINARY_SIGNATURE
// mitigation policy on Win10+. Also has other effects in ELF.
const char kAllowThirdPartyModules[] = "allow-third-party-modules";

// Add additional capabilities to the AppContainer sandbox used for XR
// compositing.
const char kAddXrAppContainerCaps[] = "add-xr-appcontainer-caps";
#endif

#if BUILDFLAG(IS_MAC)
// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[] = "enable-sandbox-logging";

// Disables Metal's shader cache, using the GPU sandbox to prevent access to it.
const char kDisableMetalShaderCache[] = "disable-metal-shader-cache";
#endif

// Flags spied upon from other layers.
const char kProcessType[] = "type";
const char kGpuProcess[] = "gpu-process";
const char kNaClLoaderProcess[] = "nacl-loader";
const char kPpapiPluginProcess[] = "ppapi";
const char kRendererProcess[] = "renderer";
const char kUtilityProcess[] = "utility";
const char kZygoteProcessType[] = "zygote";
const char kRelauncherProcessType[] = "relauncher";
const char kCodeSignCloneCleanupProcessType[] = "code-sign-clone-cleanup";

}  // namespace sandbox::policy::switches
