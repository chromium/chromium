// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SANDBOX_TYPE_H_
#define SANDBOX_POLICY_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "sandbox/policy/export.h"

namespace sandbox {
namespace policy {

// Defines the sandbox types known within the servicemanager.
enum class SandboxType {
  // Do not apply any sandboxing to the process.
  kNoSandbox,

#if defined(OS_WIN)
  // Do not apply any sandboxing and elevate the privileges of the process.
  kNoSandboxAndElevatedPrivileges,

  // The XR Compositing process.
  kXrCompositing,

  // The proxy resolver process.
  kProxyResolver,

  // The PDF conversion service process used in printing.
  kPdfConversion,

  // The icon reader service.
  kIconReader,

  // The MediaFoundation CDM service process.
  kMediaFoundationCdm,
#endif

#if defined(OS_FUCHSIA)
  // Sandbox type for the web::Context process on Fuchsia. Functionally it's an
  // equivalent of the browser process on other platforms.
  kWebContext,
#endif

  // Renderer or worker process. Most common case.
  kRenderer,

  // Utility processes. Used by most isolated services.
  kUtility,

  // GPU process.
  kGpu,

  // The PPAPI plugin process.
  kPpapi,

  // The network service process.
  kNetwork,

  // The CDM service process.
  kCdm,

#if defined(OS_MAC)
  // The NaCl loader process.
  kNaClLoader,
#endif  // defined(OS_MAC)

  // The print compositor service process.
  kPrintCompositor,

  // The audio service process.
  kAudio,

#if defined(OS_CHROMEOS)
  kIme,
  // Text-to-speech.
  kTts,
#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Indicates that a process is a zygote and will get a real sandbox later.
  kZygoteIntermediateSandbox,
#endif

#if !defined(OS_MAC)
  // Hosts WebRTC for Sharing Service, uses kUtility on OS_MAC.
  kSharingService,
#endif

  // The speech recognition service process.
  kSpeechRecognition,

  // Equivalent to no sandbox on all non-Fuchsia platforms.
  // Minimally privileged sandbox on Fuchsia.
  kVideoCapture,

  kMaxValue = kVideoCapture
};

SANDBOX_POLICY_EXPORT bool IsUnsandboxedSandboxType(SandboxType sandbox_type);

SANDBOX_POLICY_EXPORT void SetCommandLineFlagsForSandboxType(
    base::CommandLine* command_line,
    SandboxType sandbox_type);

SANDBOX_POLICY_EXPORT SandboxType
SandboxTypeFromCommandLine(const base::CommandLine& command_line);

SANDBOX_POLICY_EXPORT std::string StringFromUtilitySandboxType(
    SandboxType sandbox_type);

SANDBOX_POLICY_EXPORT SandboxType
UtilitySandboxTypeFromString(const std::string& sandbox_string);

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_SANDBOX_TYPE_H_
