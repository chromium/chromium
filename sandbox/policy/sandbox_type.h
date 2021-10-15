// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_SANDBOX_TYPE_H_
#define SANDBOX_POLICY_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
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

  // The PDF conversion service process used in printing.
  kPdfConversion,

  // The icon reader service.
  kIconReader,

  // The MediaFoundation CDM service process.
  kMediaFoundationCdm,

  // The proxy resolver process that uses WinHttp APIs.
  kWindowsSystemProxyResolver,
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

#if BUILDFLAG(ENABLE_PLUGINS)
  // The PPAPI plugin process.
  kPpapi,
#endif

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

#if BUILDFLAG(ENABLE_OOP_PRINTING)
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

#if defined(OS_FUCHSIA)
  // Equivalent to no sandbox on all non-Fuchsia platforms.
  // Minimally privileged sandbox on Fuchsia.
  kVideoCapture,
#endif  // defined(OS_FUCHSIA)

  // The speech recognition service process.
  kSpeechRecognition,

  kMaxValue = kSpeechRecognition
};

inline constexpr sandbox::policy::SandboxType MapToSandboxType(
    sandbox::mojom::Sandbox mojo_sandbox) {
  switch (mojo_sandbox) {
    case sandbox::mojom::Sandbox::kAudio:
      return sandbox::policy::SandboxType::kAudio;
    case sandbox::mojom::Sandbox::kCdm:
      return sandbox::policy::SandboxType::kCdm;
    case sandbox::mojom::Sandbox::kGpu:
      return sandbox::policy::SandboxType::kGpu;
    case sandbox::mojom::Sandbox::kNetwork:
      return sandbox::policy::SandboxType::kNetwork;
    case sandbox::mojom::Sandbox::kNoSandbox:
      return sandbox::policy::SandboxType::kNoSandbox;
    case sandbox::mojom::Sandbox::kPrintCompositor:
      return sandbox::policy::SandboxType::kPrintCompositor;
    case sandbox::mojom::Sandbox::kService:
      return sandbox::policy::SandboxType::kService;
    case sandbox::mojom::Sandbox::kSpeechRecognition:
      return sandbox::policy::SandboxType::kSpeechRecognition;
    case sandbox::mojom::Sandbox::kUtility:
      return sandbox::policy::SandboxType::kUtility;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case sandbox::mojom::Sandbox::kPrintBackend:
      return sandbox::policy::SandboxType::kPrintBackend;
#endif
#if defined(OS_FUCHSIA)
    case sandbox::mojom::Sandbox::kVideoCapture:
      return sandbox::policy::SandboxType::kVideoCapture;
#endif
#if defined(OS_WIN)
    case sandbox::mojom::Sandbox::kIconReader:
      return sandbox::policy::SandboxType::kIconReader;
    case sandbox::mojom::Sandbox::kMediaFoundationCdm:
      return sandbox::policy::SandboxType::kMediaFoundationCdm;
    case sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges:
      return sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges;
    case sandbox::mojom::Sandbox::kPdfConversion:
      return sandbox::policy::SandboxType::kPdfConversion;
    case sandbox::mojom::Sandbox::kXrCompositing:
      return sandbox::policy::SandboxType::kXrCompositing;
    case sandbox::mojom::Sandbox::kWindowsSystemProxyResolver:
      return sandbox::policy::SandboxType::kWindowsSystemProxyResolver;
#endif  // OS_WIN
#if defined(OS_MAC)
    case sandbox::mojom::Sandbox::kMirroring:
      return sandbox::policy::SandboxType::kMirroring;
#endif  // defined(OS_MAC)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case sandbox::mojom::Sandbox::kIme:
      return sandbox::policy::SandboxType::kIme;
    case sandbox::mojom::Sandbox::kTts:
      return sandbox::policy::SandboxType::kTts;
    case sandbox::mojom::Sandbox::kLibassistant:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      return sandbox::policy::SandboxType::kLibassistant;
#else
      CHECK(false) << "Libassistant sandbox not supported";
      NOTREACHED();
      return sandbox::policy::SandboxType::kService;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
