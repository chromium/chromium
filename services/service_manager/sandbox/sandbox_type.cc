// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/sandbox_type.h"

#include <string>

#include "base/feature_list.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/switches.h"

namespace service_manager {

bool IsUnsandboxedSandboxType(SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_NO_SANDBOX:
      return true;
#if defined(OS_WIN)
    case SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES:
      return true;

    case SANDBOX_TYPE_XRCOMPOSITING:
      return !base::FeatureList::IsEnabled(
          service_manager::features::kXRSandbox);
#endif
    case SANDBOX_TYPE_AUDIO:
      return !IsAudioSandboxEnabled();
    case SANDBOX_TYPE_NETWORK:
      return !base::FeatureList::IsEnabled(
          service_manager::features::kNetworkServiceSandbox);
    default:
      return false;
  }
}

void SetCommandLineFlagsForSandboxType(base::CommandLine* command_line,
                                       SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_NO_SANDBOX:
      command_line->AppendSwitch(switches::kNoSandbox);
      break;
#if defined(OS_WIN)
    case SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES:
      command_line->AppendSwitch(switches::kNoSandboxAndElevatedPrivileges);
      break;
#endif
    case SANDBOX_TYPE_RENDERER:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kRendererProcess);
      break;
    case SANDBOX_TYPE_GPU:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kGpuProcess);
      break;
    case SANDBOX_TYPE_PPAPI:
      if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess) {
        command_line->AppendSwitchASCII(switches::kServiceSandboxType,
                                        switches::kPpapiSandbox);
      } else {
        DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
               switches::kPpapiPluginProcess);
      }
      break;
    case SANDBOX_TYPE_UTILITY:
    case SANDBOX_TYPE_NETWORK:
    case SANDBOX_TYPE_CDM:
    case SANDBOX_TYPE_PDF_COMPOSITOR:
    case SANDBOX_TYPE_PROFILING:
#if defined(OS_WIN)
    case SANDBOX_TYPE_XRCOMPOSITING:
#endif
    case SANDBOX_TYPE_AUDIO:
#if defined(OS_CHROMEOS)
    case SANDBOX_TYPE_IME:
#endif  // defined(OS_CHROMEOS)
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(
          switches::kServiceSandboxType,
          StringFromUtilitySandboxType(sandbox_type));
      break;
    default:
      break;
  }
}

SandboxType SandboxTypeFromCommandLine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNoSandbox))
    return SANDBOX_TYPE_NO_SANDBOX;

#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kNoSandboxAndElevatedPrivileges))
    return SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES;
#endif

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kRendererProcess)
    return SANDBOX_TYPE_RENDERER;

  if (process_type == switches::kUtilityProcess) {
    return UtilitySandboxTypeFromString(
        command_line.GetSwitchValueASCII(switches::kServiceSandboxType));
  }
  if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return SANDBOX_TYPE_NO_SANDBOX;
    return SANDBOX_TYPE_GPU;
  }
  if (process_type == switches::kPpapiBrokerProcess)
    return SANDBOX_TYPE_NO_SANDBOX;

  if (process_type == switches::kPpapiPluginProcess)
    return SANDBOX_TYPE_PPAPI;

#if defined(OS_MACOSX)
  if (process_type == switches::kNaClLoaderProcess)
    return SANDBOX_TYPE_NACL_LOADER;
#endif

  // This is a process which we don't know about.
  return SANDBOX_TYPE_INVALID;
}

std::string StringFromUtilitySandboxType(SandboxType sandbox_type) {
  switch (sandbox_type) {
    case SANDBOX_TYPE_NO_SANDBOX:
      return switches::kNoneSandbox;
    case SANDBOX_TYPE_NETWORK:
      return switches::kNetworkSandbox;
    case SANDBOX_TYPE_PPAPI:
      return switches::kPpapiSandbox;
    case SANDBOX_TYPE_CDM:
      return switches::kCdmSandbox;
    case SANDBOX_TYPE_PDF_COMPOSITOR:
      return switches::kPdfCompositorSandbox;
    case SANDBOX_TYPE_PROFILING:
      return switches::kProfilingSandbox;
    case SANDBOX_TYPE_UTILITY:
      return switches::kUtilitySandbox;
#if defined(OS_WIN)
    case SANDBOX_TYPE_XRCOMPOSITING:
      return switches::kXrCompositingSandbox;
#endif
    case SANDBOX_TYPE_AUDIO:
      return switches::kAudioSandbox;
#if defined(OS_CHROMEOS)
    case SANDBOX_TYPE_IME:
      return switches::kImeSandbox;
#endif  // defined(OS_CHROMEOS)
    default:
      NOTREACHED();
      return std::string();
  }
}

SandboxType UtilitySandboxTypeFromString(const std::string& sandbox_string) {
  if (sandbox_string == switches::kNoneSandbox)
    return SANDBOX_TYPE_NO_SANDBOX;
  if (sandbox_string == switches::kNoneSandboxAndElevatedPrivileges) {
#if defined(OS_WIN)
    return SANDBOX_TYPE_NO_SANDBOX_AND_ELEVATED_PRIVILEGES;
#else
    return SANDBOX_TYPE_NO_SANDBOX;
#endif
  }
  if (sandbox_string == switches::kNetworkSandbox)
    return SANDBOX_TYPE_NETWORK;
  if (sandbox_string == switches::kPpapiSandbox)
    return SANDBOX_TYPE_PPAPI;
  if (sandbox_string == switches::kCdmSandbox)
    return SANDBOX_TYPE_CDM;
  if (sandbox_string == switches::kPdfCompositorSandbox)
    return SANDBOX_TYPE_PDF_COMPOSITOR;
  if (sandbox_string == switches::kProfilingSandbox)
    return SANDBOX_TYPE_PROFILING;
#if defined(OS_WIN)
  if (sandbox_string == switches::kXrCompositingSandbox)
    return SANDBOX_TYPE_XRCOMPOSITING;
#endif
  if (sandbox_string == switches::kAudioSandbox)
    return SANDBOX_TYPE_AUDIO;
#if defined(OS_CHROMEOS)
  if (sandbox_string == switches::kImeSandbox)
    return SANDBOX_TYPE_IME;
#endif  // defined(OS_CHROMEOS)
  return SANDBOX_TYPE_UTILITY;
}

void EnableAudioSandbox(bool enable) {
  if (enable) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableAudioServiceSandbox);
  }
}

bool IsAudioSandboxEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAudioServiceSandbox);
}

}  // namespace service_manager
