// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"

namespace sandbox {
namespace policy {
using sandbox::mojom::Sandbox;

bool IsUnsandboxedSandboxType(Sandbox sandbox_type) {
  switch (sandbox_type) {
    case Sandbox::kNoSandbox:
      return true;
#if defined(OS_WIN)
    case Sandbox::kNoSandboxAndElevatedPrivileges:
      return true;
    case Sandbox::kXrCompositing:
      return !base::FeatureList::IsEnabled(features::kXRSandbox);
    case Sandbox::kPdfConversion:
    case Sandbox::kIconReader:
    case Sandbox::kMediaFoundationCdm:
    case Sandbox::kWindowsSystemProxyResolver:
      return false;
#endif
    case Sandbox::kAudio:
      return false;
#if defined(OS_FUCHSIA)
    case Sandbox::kVideoCapture:
      return false;
#endif
    case Sandbox::kNetwork:
      return false;
    case Sandbox::kRenderer:
    case Sandbox::kService:
    case Sandbox::kUtility:
    case Sandbox::kGpu:
#if BUILDFLAG(ENABLE_PLUGINS)
    case Sandbox::kPpapi:
#endif
    case Sandbox::kCdm:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case Sandbox::kPrintBackend:
#endif
    case Sandbox::kPrintCompositor:
#if defined(OS_MAC)
    case Sandbox::kMirroring:
    case Sandbox::kNaClLoader:
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Sandbox::kIme:
    case Sandbox::kTts:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case Sandbox::kLibassistant:
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    case Sandbox::kZygoteIntermediateSandbox:
#endif
    case Sandbox::kSpeechRecognition:
      return false;
  }
}

void SetCommandLineFlagsForSandboxType(base::CommandLine* command_line,
                                       Sandbox sandbox_type) {
  switch (sandbox_type) {
    case Sandbox::kNoSandbox:
      if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess) {
        DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
        command_line->AppendSwitchASCII(
            switches::kServiceSandboxType,
            StringFromUtilitySandboxType(sandbox_type));
      } else {
        command_line->AppendSwitch(switches::kNoSandbox);
      }
      break;
    case Sandbox::kRenderer:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kRendererProcess);
      break;
    case Sandbox::kGpu:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kGpuProcess);
      break;
#if BUILDFLAG(ENABLE_PLUGINS)
    case Sandbox::kPpapi:
      if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess) {
        command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                        switches::kPpapiSandbox);
      } else {
        DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
               switches::kPpapiPluginProcess);
      }
      break;
#endif
    case Sandbox::kService:
    case Sandbox::kUtility:
    case Sandbox::kNetwork:
    case Sandbox::kCdm:
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case Sandbox::kPrintBackend:
#endif
    case Sandbox::kPrintCompositor:
    case Sandbox::kAudio:
#if defined(OS_FUCHSIA)
    case Sandbox::kVideoCapture:
#endif
#if defined(OS_WIN)
    case Sandbox::kNoSandboxAndElevatedPrivileges:
    case Sandbox::kXrCompositing:
    case Sandbox::kPdfConversion:
    case Sandbox::kIconReader:
    case Sandbox::kMediaFoundationCdm:
    case Sandbox::kWindowsSystemProxyResolver:
#endif  // defined(OS_WIN)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Sandbox::kIme:
    case Sandbox::kTts:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case Sandbox::kLibassistant:
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_MAC)
    case Sandbox::kMirroring:
#endif  // defined(OS_MAC)
    case Sandbox::kSpeechRecognition:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(
          switches::kServiceSandboxType,
          StringFromUtilitySandboxType(sandbox_type));
      break;
#if defined(OS_MAC)
    case Sandbox::kNaClLoader:
      break;
#endif  // defined(OS_MAC)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    case Sandbox::kZygoteIntermediateSandbox:
      break;
#endif
  }
}

sandbox::mojom::Sandbox SandboxTypeFromCommandLine(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return Sandbox::kNoSandbox;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return Sandbox::kNoSandbox;

  if (process_type == switches::kRendererProcess)
    return Sandbox::kRenderer;

  if (process_type == switches::kUtilityProcess) {
    return UtilitySandboxTypeFromString(
        command_line.GetSwitchValueASCII(switches::kServiceSandboxType));
  }
  if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return Sandbox::kNoSandbox;
    return Sandbox::kGpu;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  if (process_type == switches::kPpapiPluginProcess)
    return Sandbox::kPpapi;
#endif

  // NaCl tests on all platforms use the loader process.
  if (process_type == switches::kNaClLoaderProcess) {
#if defined(OS_MAC)
    return Sandbox::kNaClLoader;
#else
    return Sandbox::kUtility;
#endif
  }

  if (process_type == switches::kNaClBrokerProcess)
    return Sandbox::kNoSandbox;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Intermediate process gains a sandbox later.
  if (process_type == switches::kZygoteProcessType)
    return Sandbox::kZygoteIntermediateSandbox;
#endif

#if defined(OS_MAC)
  if (process_type == switches::kRelauncherProcessType)
    return Sandbox::kNoSandbox;
#endif

  if (process_type == switches::kCloudPrintServiceProcess)
    return Sandbox::kNoSandbox;

  CHECK(false)
      << "Command line does not provide a valid sandbox configuration: "
      << command_line.GetCommandLineString();
  NOTREACHED();
  return Sandbox::kNoSandbox;
}

std::string StringFromUtilitySandboxType(Sandbox sandbox_type) {
  switch (sandbox_type) {
    case Sandbox::kNoSandbox:
      return switches::kNoneSandbox;
#if defined(OS_WIN)
    case Sandbox::kNoSandboxAndElevatedPrivileges:
      return switches::kNoneSandboxAndElevatedPrivileges;
#endif  // defined(OS_WIN)
    case Sandbox::kNetwork:
      return switches::kNetworkSandbox;
#if BUILDFLAG(ENABLE_PLUGINS)
    case Sandbox::kPpapi:
      return switches::kPpapiSandbox;
#endif
    case Sandbox::kCdm:
      return switches::kCdmSandbox;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    case Sandbox::kPrintBackend:
      return switches::kPrintBackendSandbox;
#endif
    case Sandbox::kPrintCompositor:
      return switches::kPrintCompositorSandbox;
    case Sandbox::kUtility:
      return switches::kUtilitySandbox;
    case Sandbox::kAudio:
      return switches::kAudioSandbox;
#if defined(OS_FUCHSIA)
    case Sandbox::kVideoCapture:
      return switches::kVideoCaptureSandbox;
#endif
    case Sandbox::kService:
      return switches::kServiceSandbox;
    case Sandbox::kSpeechRecognition:
      return switches::kSpeechRecognitionSandbox;
#if defined(OS_WIN)
    case Sandbox::kXrCompositing:
      return switches::kXrCompositingSandbox;
    case Sandbox::kPdfConversion:
      return switches::kPdfConversionSandbox;
    case Sandbox::kIconReader:
      return switches::kIconReaderSandbox;
    case Sandbox::kMediaFoundationCdm:
      return switches::kMediaFoundationCdmSandbox;
    case Sandbox::kWindowsSystemProxyResolver:
      return switches::kWindowsSystemProxyResolverSandbox;
#endif  // defined(OS_WIN)
#if defined(OS_MAC)
    case Sandbox::kMirroring:
      return switches::kMirroringSandbox;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Sandbox::kIme:
      return switches::kImeSandbox;
    case Sandbox::kTts:
      return switches::kTtsSandbox;
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
    case Sandbox::kLibassistant:
      return switches::kLibassistantSandbox;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      // The following are not utility processes so should not occur.
    case Sandbox::kRenderer:
    case Sandbox::kGpu:
#if defined(OS_MAC)
    case Sandbox::kNaClLoader:
#endif  // defined(OS_MAC)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    case Sandbox::kZygoteIntermediateSandbox:
#endif
      NOTREACHED();
      return std::string();
  }
}

sandbox::mojom::Sandbox UtilitySandboxTypeFromString(
    const std::string& sandbox_string) {
  // This function should cover all sandbox types used for utilities, the
  // CHECK at the end should catch any attempts to forget to add a new type.

  // Most utilities are kUtility or kService so put those first.
  if (sandbox_string == switches::kUtilitySandbox)
    return Sandbox::kUtility;
  if (sandbox_string == switches::kServiceSandbox)
    return Sandbox::kService;

  if (sandbox_string == switches::kNoneSandbox)
    return Sandbox::kNoSandbox;
  if (sandbox_string == switches::kNoneSandboxAndElevatedPrivileges) {
#if defined(OS_WIN)
    return Sandbox::kNoSandboxAndElevatedPrivileges;
#else
    return Sandbox::kNoSandbox;
#endif
  }
  if (sandbox_string == switches::kNetworkSandbox)
    return Sandbox::kNetwork;
#if BUILDFLAG(ENABLE_PLUGINS)
  if (sandbox_string == switches::kPpapiSandbox)
    return Sandbox::kPpapi;
#endif
  if (sandbox_string == switches::kCdmSandbox)
    return Sandbox::kCdm;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (sandbox_string == switches::kPrintBackendSandbox)
    return Sandbox::kPrintBackend;
#endif
  if (sandbox_string == switches::kPrintCompositorSandbox)
    return Sandbox::kPrintCompositor;
#if defined(OS_WIN)
  if (sandbox_string == switches::kXrCompositingSandbox)
    return Sandbox::kXrCompositing;
  if (sandbox_string == switches::kPdfConversionSandbox)
    return Sandbox::kPdfConversion;
  if (sandbox_string == switches::kIconReaderSandbox)
    return Sandbox::kIconReader;
  if (sandbox_string == switches::kMediaFoundationCdmSandbox)
    return Sandbox::kMediaFoundationCdm;
  if (sandbox_string == switches::kWindowsSystemProxyResolverSandbox)
    return Sandbox::kWindowsSystemProxyResolver;
#endif
#if defined(OS_MAC)
  if (sandbox_string == switches::kMirroringSandbox)
    return Sandbox::kMirroring;
#endif
  if (sandbox_string == switches::kAudioSandbox)
    return Sandbox::kAudio;
  if (sandbox_string == switches::kSpeechRecognitionSandbox)
    return Sandbox::kSpeechRecognition;
#if defined(OS_FUCHSIA)
  if (sandbox_string == switches::kVideoCaptureSandbox)
    return Sandbox::kVideoCapture;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (sandbox_string == switches::kImeSandbox)
    return Sandbox::kIme;
  if (sandbox_string == switches::kTtsSandbox)
    return Sandbox::kTts;
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  if (sandbox_string == switches::kLibassistantSandbox)
    return Sandbox::kLibassistant;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(false)
      << "Command line does not provide a valid sandbox configuration: "
      << sandbox_string;
  NOTREACHED();
  return Sandbox::kUtility;
}

}  // namespace policy
}  // namespace sandbox
