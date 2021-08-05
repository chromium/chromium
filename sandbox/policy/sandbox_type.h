// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SANDBOX_TYPE_H_
#define SANDBOX_POLICY_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  // Renderer or worker process. Most common case.
  kRenderer,

  // Utility processes. Used by most isolated services.  Consider using
  // kService for Chromium-code that makes limited use of OS APIs.
  kUtility,

#if defined(OS_MAC)
  // On Mac these are identical.
  kService = kUtility,
#else
  // Services with limited use of OS APIs. Tighter than kUtility and
  // suitable for most isolated mojo service endpoints.
  kService,
#endif

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

  // The mirroring service needs IOSurface access on macOS.
  kMirroring,
#endif  // defined(OS_MAC)

#if BUILDFLAG(ENABLE_PRINTING)
  // The print backend service process which interfaces with operating system
  // print drivers.
  kPrintBackend,
#endif

  // The print compositor service process.
  kPrintCompositor,

  // The audio service process.
  kAudio,

#if BUILDFLAG(IS_CHROMEOS_ASH)
  kIme,
  // Text-to-speech.
  kTts,

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  kLibassistant,
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Indicates that a process is a zygote and will get a real sandbox later.
  kZygoteIntermediateSandbox,
#endif

  // The speech recognition service process.
  kSpeechRecognition,

  // Equivalent to no sandbox on all non-Fuchsia platforms.
  // Minimally privileged sandbox on Fuchsia.
  kVideoCapture,

  kMaxValue = kVideoCapture
};

inline constexpr sandbox::policy::SandboxType MapToSandboxType(
    sandbox::mojom::Sandbox mojo_sandbox) {
  switch (mojo_sandbox) {
    case sandbox::mojom::Sandbox::kService:
      return sandbox::policy::SandboxType::kService;
    case sandbox::mojom::Sandbox::kUtility:
      return sandbox::policy::SandboxType::kUtility;
#if defined(OS_WIN)
    case sandbox::mojom::Sandbox::kXrCompositing:
      return sandbox::policy::SandboxType::kXrCompositing;
#endif  // OS_WIN
  }
}

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
