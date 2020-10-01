// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SWITCHES_H_
#define SANDBOX_POLICY_SWITCHES_H_

#include "build/build_config.h"
#include "sandbox/policy/export.h"

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
SANDBOX_POLICY_EXPORT extern const char kPpapiSandbox[];
SANDBOX_POLICY_EXPORT extern const char kUtilitySandbox[];
SANDBOX_POLICY_EXPORT extern const char kCdmSandbox[];
SANDBOX_POLICY_EXPORT extern const char kPrintCompositorSandbox[];
SANDBOX_POLICY_EXPORT extern const char kAudioSandbox[];
SANDBOX_POLICY_EXPORT extern const char kSharingServiceSandbox[];
SANDBOX_POLICY_EXPORT extern const char kSpeechRecognitionSandbox[];
SANDBOX_POLICY_EXPORT extern const char kVideoCaptureSandbox[];

#if defined(OS_WIN)
SANDBOX_POLICY_EXPORT extern const char kPdfConversionSandbox[];
SANDBOX_POLICY_EXPORT extern const char kProxyResolverSandbox[];
SANDBOX_POLICY_EXPORT extern const char kXrCompositingSandbox[];
SANDBOX_POLICY_EXPORT extern const char kIconReaderSandbox[];
SANDBOX_POLICY_EXPORT extern const char kMediaFoundationCdmSandbox[];
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
SANDBOX_POLICY_EXPORT extern const char kImeSandbox[];
SANDBOX_POLICY_EXPORT extern const char kTtsSandbox[];
#endif  // OS_CHROMEOS

// Flags owned by the service manager sandbox.
SANDBOX_POLICY_EXPORT extern const char kAllowNoSandboxJob[];
SANDBOX_POLICY_EXPORT extern const char kAllowSandboxDebugging[];
SANDBOX_POLICY_EXPORT extern const char kDisableGpuSandbox[];
SANDBOX_POLICY_EXPORT extern const char kDisableNamespaceSandbox[];
SANDBOX_POLICY_EXPORT extern const char kDisableSeccompFilterSandbox[];
SANDBOX_POLICY_EXPORT extern const char kDisableSetuidSandbox[];
SANDBOX_POLICY_EXPORT extern const char kGpuSandboxAllowSysVShm[];
SANDBOX_POLICY_EXPORT extern const char kGpuSandboxFailuresFatal[];
SANDBOX_POLICY_EXPORT extern const char kNoSandbox[];
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
SANDBOX_POLICY_EXPORT extern const char kNoZygoteSandbox[];
#endif
#if defined(OS_WIN)
SANDBOX_POLICY_EXPORT extern const char kAllowThirdPartyModules[];
SANDBOX_POLICY_EXPORT extern const char kAddGpuAppContainerCaps[];
SANDBOX_POLICY_EXPORT extern const char kNoSandboxAndElevatedPrivileges[];
SANDBOX_POLICY_EXPORT extern const char kAddXrAppContainerCaps[];
#endif
#if defined(OS_MAC)
SANDBOX_POLICY_EXPORT extern const char kEnableSandboxLogging[];
#endif

// Flags spied upon from other layers.
SANDBOX_POLICY_EXPORT extern const char kProcessType[];
SANDBOX_POLICY_EXPORT extern const char kGpuProcess[];
SANDBOX_POLICY_EXPORT extern const char kNaClBrokerProcess[];
SANDBOX_POLICY_EXPORT extern const char kNaClLoaderProcess[];
SANDBOX_POLICY_EXPORT extern const char kPpapiBrokerProcess[];
SANDBOX_POLICY_EXPORT extern const char kPpapiPluginProcess[];
SANDBOX_POLICY_EXPORT extern const char kRendererProcess[];
SANDBOX_POLICY_EXPORT extern const char kUtilityProcess[];
SANDBOX_POLICY_EXPORT extern const char kCloudPrintServiceProcess[];
SANDBOX_POLICY_EXPORT extern const char kZygoteProcessType[];

}  // namespace switches
}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_SWITCHES_H_
