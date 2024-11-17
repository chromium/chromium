// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SWITCHES_H_
#define SANDBOX_POLICY_SWITCHES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/export.h"
#include "services/screen_ai/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sandbox {
namespace policy {
namespace switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
SANDBOX_POLICY_EXPORT extern const char kServiceSandboxType[];

// Must be in sync with "sandbox_type" values as used in service manager's
// manifest.json catalog files.
SANDBOX_POLICY_EXPORT extern const char kNoneSandbox[];
SANDBOX_POLICY_EXPORT extern const char kNoneSandboxAndElevatedPrivileges[];
SANDBOX_POLICY_EXPORT extern const char kNetworkSandbox[];
SANDBOX_POLICY_EXPORT extern const char kOnDeviceModelExecutionSandbox[];
SANDBOX_POLICY_EXPORT extern const char kPpapiSandbox[];
SANDBOX_POLICY_EXPORT extern const char kUtilitySandbox[];
SANDBOX_POLICY_EXPORT extern const char kCdmSandbox[];
#if BUILDFLAG(ENABLE_PRINTING)
SANDBOX_POLICY_EXPORT extern const char kPrintBackendSandbox[];
#endif
SANDBOX_POLICY_EXPORT extern const char kPrintCompositorSandbox[];
SANDBOX_POLICY_EXPORT extern const char kAudioSandbox[];
SANDBOX_POLICY_EXPORT extern const char kServiceSandbox[];
SANDBOX_POLICY_EXPORT extern const char kServiceSandboxWithJit[];
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
SANDBOX_POLICY_EXPORT extern const char kScreenAISandbox[];
#endif
SANDBOX_POLICY_EXPORT extern const char kVideoEffectsSandbox[];
SANDBOX_POLICY_EXPORT extern const char kSpeechRecognitionSandbox[];
SANDBOX_POLICY_EXPORT extern const char kVideoCaptureSandbox[];

#if BUILDFLAG(IS_WIN)
SANDBOX_POLICY_EXPORT extern const char kPdfConversionSandbox[];
SANDBOX_POLICY_EXPORT extern const char kXrCompositingSandbox[];
SANDBOX_POLICY_EXPORT extern const char kIconReaderSandbox[];
SANDBOX_POLICY_EXPORT extern const char kMediaFoundationCdmSandbox[];
SANDBOX_POLICY_EXPORT extern const char kWindowsSystemProxyResolverSandbox[];
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
SANDBOX_POLICY_EXPORT extern const char kMirroringSandbox[];
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
SANDBOX_POLICY_EXPORT extern const char kHardwareVideoDecodingSandbox[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
SANDBOX_POLICY_EXPORT extern const char kHardwareVideoEncodingSandbox[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
SANDBOX_POLICY_EXPORT extern const char kImeSandbox[];
SANDBOX_POLICY_EXPORT extern const char kTtsSandbox[];
SANDBOX_POLICY_EXPORT extern const char kNearbySandbox[];
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
SANDBOX_POLICY_EXPORT extern const char kLibassistantSandbox[];
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
SANDBOX_POLICY_EXPORT extern const char kOnDeviceTranslationSandbox[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

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
SANDBOX_POLICY_EXPORT extern const char kAddGpuAppContainerCaps[];
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

}  // namespace switches
}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_SWITCHES_H_
