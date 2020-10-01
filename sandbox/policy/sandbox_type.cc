// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"

namespace sandbox {
namespace policy {

bool IsUnsandboxedSandboxType(SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SandboxType::kNoSandbox:
      return true;
#if defined(OS_WIN)
    case SandboxType::kNoSandboxAndElevatedPrivileges:
      return true;
    case SandboxType::kXrCompositing:
      return !base::FeatureList::IsEnabled(features::kXRSandbox);
    case SandboxType::kProxyResolver:
    case SandboxType::kPdfConversion:
    case SandboxType::kIconReader:
    case SandboxType::kMediaFoundationCdm:
      return false;
#endif
    case SandboxType::kAudio:
      return false;
    case SandboxType::kVideoCapture:
#if defined(OS_FUCHSIA)
      return false;
#else
      return true;
#endif
    case SandboxType::kNetwork:
#if defined(OS_MAC)
      return false;
#else
      return !base::FeatureList::IsEnabled(features::kNetworkServiceSandbox);
#endif  // defined(OS_MAC)
    case SandboxType::kRenderer:
    case SandboxType::kUtility:
    case SandboxType::kGpu:
    case SandboxType::kPpapi:
    case SandboxType::kCdm:
    case SandboxType::kPrintCompositor:
#if defined(OS_FUCHSIA)
    case SandboxType::kWebContext:
#endif
#if defined(OS_MAC)
    case SandboxType::kNaClLoader:
#endif
#if defined(OS_CHROMEOS)
    case SandboxType::kIme:
    case SandboxType::kTts:
#endif
#if !defined(OS_MAC)
    case SandboxType::kSharingService:
#endif
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    case SandboxType::kZygoteIntermediateSandbox:
#endif
    case SandboxType::kSpeechRecognition:
      return false;
  }
}

void SetCommandLineFlagsForSandboxType(base::CommandLine* command_line,
                                       SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SandboxType::kNoSandbox:
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
#if defined(OS_WIN)
    case SandboxType::kNoSandboxAndElevatedPrivileges:
      command_line->AppendSwitch(switches::kNoSandboxAndElevatedPrivileges);
      break;
#endif
    case SandboxType::kRenderer:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kRendererProcess);
      break;
    case SandboxType::kGpu:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kGpuProcess);
      break;
    case SandboxType::kPpapi:
      if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess) {
        command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                        switches::kPpapiSandbox);
      } else {
        DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
               switches::kPpapiPluginProcess);
      }
      break;
    case SandboxType::kUtility:
    case SandboxType::kNetwork:
    case SandboxType::kCdm:
    case SandboxType::kPrintCompositor:
    case SandboxType::kAudio:
    case SandboxType::kVideoCapture:
#if defined(OS_WIN)
    case SandboxType::kXrCompositing:
    case SandboxType::kProxyResolver:
    case SandboxType::kPdfConversion:
    case SandboxType::kIconReader:
    case SandboxType::kMediaFoundationCdm:
#endif  // defined(OS_WIN)
#if defined(OS_CHROMEOS)
    case SandboxType::kIme:
    case SandboxType::kTts:
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_MAC)
    case SandboxType::kSharingService:
#endif
    case SandboxType::kSpeechRecognition:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(
          switches::kServiceSandboxType,
          StringFromUtilitySandboxType(sandbox_type));
      break;
#if defined(OS_FUCHSIA)
    case SandboxType::kWebContext:
      break;
#endif  // defined(OS_FUCHSIA)
#if defined(OS_MAC)
    case SandboxType::kNaClLoader:
      break;
#endif  // defined(OS_MAC)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    case SandboxType::kZygoteIntermediateSandbox:
      break;
#endif
  }
}

SandboxType SandboxTypeFromCommandLine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return SandboxType::kNoSandbox;

#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kNoSandboxAndElevatedPrivileges))
    return SandboxType::kNoSandboxAndElevatedPrivileges;
#endif

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return SandboxType::kNoSandbox;

  if (process_type == switches::kRendererProcess)
    return SandboxType::kRenderer;

  if (process_type == switches::kUtilityProcess) {
    return UtilitySandboxTypeFromString(
        command_line.GetSwitchValueASCII(switches::kServiceSandboxType));
  }
  if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return SandboxType::kNoSandbox;
    return SandboxType::kGpu;
  }
  if (process_type == switches::kPpapiBrokerProcess)
    return SandboxType::kNoSandbox;

  if (process_type == switches::kPpapiPluginProcess)
    return SandboxType::kPpapi;

  // NaCl tests on all platforms use the loader process.
  if (process_type == switches::kNaClLoaderProcess) {
#if defined(OS_MAC)
    return SandboxType::kNaClLoader;
#else
    return SandboxType::kUtility;
#endif
  }

  if (process_type == switches::kNaClBrokerProcess)
    return SandboxType::kNoSandbox;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Intermediate process gains a sandbox later.
  if (process_type == switches::kZygoteProcessType)
    return SandboxType::kZygoteIntermediateSandbox;
#endif

  if (process_type == switches::kCloudPrintServiceProcess)
    return SandboxType::kNoSandbox;

  CHECK(false)
      << "Command line does not provide a valid sandbox configuration: "
      << command_line.GetCommandLineString();
  NOTREACHED();
  return SandboxType::kNoSandbox;
}

std::string StringFromUtilitySandboxType(SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SandboxType::kNoSandbox:
      return switches::kNoneSandbox;
    case SandboxType::kNetwork:
      return switches::kNetworkSandbox;
    case SandboxType::kPpapi:
      return switches::kPpapiSandbox;
    case SandboxType::kCdm:
      return switches::kCdmSandbox;
    case SandboxType::kPrintCompositor:
      return switches::kPrintCompositorSandbox;
    case SandboxType::kUtility:
      return switches::kUtilitySandbox;
    case SandboxType::kAudio:
      return switches::kAudioSandbox;
    case SandboxType::kVideoCapture:
      return switches::kVideoCaptureSandbox;
#if !defined(OS_MAC)
    case SandboxType::kSharingService:
      return switches::kSharingServiceSandbox;
#endif
    case SandboxType::kSpeechRecognition:
      return switches::kSpeechRecognitionSandbox;
#if defined(OS_WIN)
    case SandboxType::kXrCompositing:
      return switches::kXrCompositingSandbox;
    case SandboxType::kProxyResolver:
      return switches::kProxyResolverSandbox;
    case SandboxType::kPdfConversion:
      return switches::kPdfConversionSandbox;
    case SandboxType::kIconReader:
      return switches::kIconReaderSandbox;
    case SandboxType::kMediaFoundationCdm:
      return switches::kMediaFoundationCdmSandbox;
#endif  // defined(OS_WIN)
#if defined(OS_CHROMEOS)
    case SandboxType::kIme:
      return switches::kImeSandbox;
    case SandboxType::kTts:
      return switches::kTtsSandbox;
#endif  // defined(OS_CHROMEOS)
      // The following are not utility processes so should not occur.
    case SandboxType::kRenderer:
    case SandboxType::kGpu:
#if defined(OS_WIN)
    case SandboxType::kNoSandboxAndElevatedPrivileges:
#endif  // defined(OS_WIN)
#if defined(OS_MAC)
    case SandboxType::kNaClLoader:
#endif  // defined(OS_MAC)
#if defined(OS_FUCHSIA)
    case SandboxType::kWebContext:
#endif  // defined(OS_FUCHSIA)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    case SandboxType::kZygoteIntermediateSandbox:
#endif
      NOTREACHED();
      return std::string();
  }
}

SandboxType UtilitySandboxTypeFromString(const std::string& sandbox_string) {
  if (sandbox_string == switches::kNoneSandbox)
    return SandboxType::kNoSandbox;
  if (sandbox_string == switches::kNoneSandboxAndElevatedPrivileges) {
#if defined(OS_WIN)
    return SandboxType::kNoSandboxAndElevatedPrivileges;
#else
    return SandboxType::kNoSandbox;
#endif
  }
  if (sandbox_string == switches::kNetworkSandbox)
    return SandboxType::kNetwork;
  if (sandbox_string == switches::kPpapiSandbox)
    return SandboxType::kPpapi;
  if (sandbox_string == switches::kCdmSandbox)
    return SandboxType::kCdm;
  if (sandbox_string == switches::kPrintCompositorSandbox)
    return SandboxType::kPrintCompositor;
#if defined(OS_WIN)
  if (sandbox_string == switches::kXrCompositingSandbox)
    return SandboxType::kXrCompositing;
  if (sandbox_string == switches::kProxyResolverSandbox)
    return SandboxType::kProxyResolver;
  if (sandbox_string == switches::kPdfConversionSandbox)
    return SandboxType::kPdfConversion;
  if (sandbox_string == switches::kIconReaderSandbox)
    return SandboxType::kIconReader;
  if (sandbox_string == switches::kMediaFoundationCdmSandbox)
    return SandboxType::kMediaFoundationCdm;
#endif
  if (sandbox_string == switches::kAudioSandbox)
    return SandboxType::kAudio;
  if (sandbox_string == switches::kSpeechRecognitionSandbox)
    return SandboxType::kSpeechRecognition;
  if (sandbox_string == switches::kVideoCaptureSandbox)
    return SandboxType::kVideoCapture;
#if defined(OS_CHROMEOS)
  if (sandbox_string == switches::kImeSandbox)
    return SandboxType::kIme;
  if (sandbox_string == switches::kTtsSandbox)
    return SandboxType::kTts;
#endif  // defined(OS_CHROMEOS)
  return SandboxType::kUtility;
}

}  // namespace policy
}  // namespace sandbox
