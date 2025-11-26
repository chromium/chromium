// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "media/gpu/buildflags.h"  // nogncheck
#include "media/media_buildflags.h"  // nogncheck
#endif

namespace sandbox::policy {

namespace {

// Switch values that are only accessed in this file.
constexpr char kNoneSandbox[] = "none";
constexpr char kNetworkSandbox[] = "network";
constexpr char kOnDeviceModelExecutionSandbox[] = "on_device_model_execution";
constexpr char kUtilitySandbox[] = "utility";
constexpr char kCdmSandbox[] = "cdm";
constexpr char kPrintCompositorSandbox[] = "print_compositor";
constexpr char kAudioSandbox[] = "audio";
constexpr char kServiceSandbox[] = "service";
constexpr char kServiceSandboxWithJit[] = "service_with_jit";
constexpr char kSpeechRecognitionSandbox[] = "speech_recognition";

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
constexpr char kPrintBackendSandbox[] = "print_backend";
constexpr char kScreenAISandbox[] = "screen_ai";
#endif

#if BUILDFLAG(IS_WIN)
constexpr char kNoneSandboxAndElevatedPrivileges[] = "none_and_elevated";
constexpr char kPdfConversionSandbox[] = "pdf_conversion";
constexpr char kXrCompositingSandbox[] = "xr_compositing";
constexpr char kIconReaderSandbox[] = "icon_reader";
constexpr char kMediaFoundationCdmSandbox[] = "mf_cdm";
constexpr char kProxyResolverSandbox[] = "proxy_resolver";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
constexpr char kMirroringSandbox[] = "mirroring";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_FUCHSIA)
constexpr char kVideoCaptureSandbox[] = "video_capture";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
constexpr char kShapeDetectionSandbox[] = "shape_detection";
// USE_LINUX_VIDEO_ACCELERATION implies IS_LINUX || IS_CHROMEOS, so this double
// #if is redundant, however, we cannot include "media/gpu/buildflags.h" on all
// platforms, only one those that need to evaluate the use..., hence this
// pattern, here and elsewhere. This problem is specific to this file.
#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
constexpr char kHardwareVideoDecodingSandbox[] = "hardware_video_decoding";
constexpr char kHardwareVideoEncodingSandbox[] = "hardware_video_encoding";
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kImeSandbox[] = "ime";
constexpr char kTtsSandbox[] = "tts";
constexpr char kNearbySandbox[] = "nearby";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
constexpr char kOnDeviceTranslationSandbox[] = "on_device_translation";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

using sandbox::mojom::Sandbox;

bool IsUnsandboxedSandboxType(Sandbox sandbox_type) {
  if (sandbox_type == Sandbox::kNoSandbox) {
    return true;
  }
#if BUILDFLAG(IS_WIN)
  if (sandbox_type == Sandbox::kNoSandboxAndElevatedPrivileges) {
    return true;
  }
#endif
  return false;
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
      return;
    case Sandbox::kRenderer:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kRendererProcess);
      return;
    case Sandbox::kGpu:
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kGpuProcess);
      return;
    case Sandbox::kService:
    case Sandbox::kServiceWithJit:
    case Sandbox::kUtility:
    case Sandbox::kNetwork:
    case Sandbox::kOnDeviceModelExecution:
    case Sandbox::kCdm:
    case Sandbox::kPrintCompositor:
    case Sandbox::kAudio:
#if BUILDFLAG(IS_FUCHSIA)
    case Sandbox::kVideoCapture:
#endif
#if BUILDFLAG(IS_WIN)
    case Sandbox::kNoSandboxAndElevatedPrivileges:
    case Sandbox::kXrCompositing:
    case Sandbox::kPdfConversion:
    case Sandbox::kIconReader:
    case Sandbox::kMediaFoundationCdm:
    case Sandbox::kProxyResolver:
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kShapeDetection:
#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
    case Sandbox::kHardwareVideoDecoding:
    case Sandbox::kHardwareVideoEncoding:
#endif
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kIme:
    case Sandbox::kTts:
    case Sandbox::kNearby:
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_MAC)
    case Sandbox::kMirroring:
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    case Sandbox::kPrintBackend:
    case Sandbox::kScreenAI:
#endif
    case Sandbox::kSpeechRecognition:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kOnDeviceTranslation:
#endif
      DCHECK(command_line->GetSwitchValueASCII(switches::kProcessType) ==
             switches::kUtilityProcess);
      DCHECK(!command_line->HasSwitch(switches::kServiceSandboxType));
      command_line->AppendSwitchASCII(
          switches::kServiceSandboxType,
          StringFromUtilitySandboxType(sandbox_type));
      return;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kZygoteIntermediateSandbox:
      return;
#endif
  }
  NOTREACHED();
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Intermediate process gains a sandbox later.
  if (process_type == switches::kZygoteProcessType)
    return Sandbox::kZygoteIntermediateSandbox;
#endif

#if BUILDFLAG(IS_MAC)
  if (process_type == switches::kRelauncherProcessType ||
      process_type == switches::kCodeSignCloneCleanupProcessType) {
    return Sandbox::kNoSandbox;
  }
#endif

  NOTREACHED()
      << "Command line does not provide a valid sandbox configuration: "
      << command_line.GetCommandLineString();
}

std::string StringFromUtilitySandboxType(Sandbox sandbox_type) {
  switch (sandbox_type) {
    case Sandbox::kNoSandbox:
      return kNoneSandbox;
#if BUILDFLAG(IS_WIN)
    case Sandbox::kNoSandboxAndElevatedPrivileges:
      return kNoneSandboxAndElevatedPrivileges;
#endif  // BUILDFLAG(IS_WIN)
    case Sandbox::kNetwork:
      return kNetworkSandbox;
    case Sandbox::kOnDeviceModelExecution:
      return kOnDeviceModelExecutionSandbox;
    case Sandbox::kCdm:
      return kCdmSandbox;
    case Sandbox::kPrintCompositor:
      return kPrintCompositorSandbox;
    case Sandbox::kUtility:
      return kUtilitySandbox;
    case Sandbox::kAudio:
      return kAudioSandbox;
#if BUILDFLAG(IS_FUCHSIA)
    case Sandbox::kVideoCapture:
      return kVideoCaptureSandbox;
#endif
    case Sandbox::kService:
      return kServiceSandbox;
    case Sandbox::kServiceWithJit:
      return kServiceSandboxWithJit;
    case Sandbox::kSpeechRecognition:
      return kSpeechRecognitionSandbox;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    case Sandbox::kPrintBackend:
      return kPrintBackendSandbox;
    case Sandbox::kScreenAI:
      return kScreenAISandbox;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kOnDeviceTranslation:
      return kOnDeviceTranslationSandbox;
#endif
#if BUILDFLAG(IS_WIN)
    case Sandbox::kXrCompositing:
      return kXrCompositingSandbox;
    case Sandbox::kPdfConversion:
      return kPdfConversionSandbox;
    case Sandbox::kIconReader:
      return kIconReaderSandbox;
    case Sandbox::kMediaFoundationCdm:
      return kMediaFoundationCdmSandbox;
    case Sandbox::kProxyResolver:
      return kProxyResolverSandbox;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_MAC)
    case Sandbox::kMirroring:
      return kMirroringSandbox;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kShapeDetection:
      return kShapeDetectionSandbox;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
    case Sandbox::kHardwareVideoDecoding:
      return kHardwareVideoDecodingSandbox;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
    case Sandbox::kHardwareVideoEncoding:
      return kHardwareVideoEncodingSandbox;
#endif  // BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kIme:
      return kImeSandbox;
    case Sandbox::kTts:
      return kTtsSandbox;
    case Sandbox::kNearby:
      return kNearbySandbox;
#endif  // BUILDFLAG(IS_CHROMEOS)
      // The following are not utility processes so should not occur.
    case Sandbox::kRenderer:
    case Sandbox::kGpu:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    case Sandbox::kZygoteIntermediateSandbox:
#endif
      NOTREACHED();
  }
  NOTREACHED();
}

sandbox::mojom::Sandbox UtilitySandboxTypeFromString(
    const std::string& sandbox_string) {
  // This function should cover all sandbox types used for utilities, the
  // CHECK at the end should catch any attempts to forget to add a new type.

  // Most utilities are kUtility or kService so put those first.
  if (sandbox_string == kUtilitySandbox) {
    return Sandbox::kUtility;
  }
  if (sandbox_string == kServiceSandbox) {
    return Sandbox::kService;
  }
  if (sandbox_string == kServiceSandboxWithJit) {
    return Sandbox::kServiceWithJit;
  }

  if (sandbox_string == kNoneSandbox) {
    return Sandbox::kNoSandbox;
  }
#if BUILDFLAG(IS_WIN)
  if (sandbox_string == kNoneSandboxAndElevatedPrivileges) {
    return Sandbox::kNoSandboxAndElevatedPrivileges;
  }
#endif

  if (sandbox_string == kNetworkSandbox) {
    return Sandbox::kNetwork;
  }
  if (sandbox_string == kOnDeviceModelExecutionSandbox) {
    return Sandbox::kOnDeviceModelExecution;
  }
  if (sandbox_string == kCdmSandbox) {
    return Sandbox::kCdm;
  }
  if (sandbox_string == kPrintCompositorSandbox) {
    return Sandbox::kPrintCompositor;
  }
#if BUILDFLAG(IS_WIN)
  if (sandbox_string == kXrCompositingSandbox) {
    return Sandbox::kXrCompositing;
  }
  if (sandbox_string == kPdfConversionSandbox) {
    return Sandbox::kPdfConversion;
  }
  if (sandbox_string == kIconReaderSandbox) {
    return Sandbox::kIconReader;
  }
  if (sandbox_string == kMediaFoundationCdmSandbox) {
    return Sandbox::kMediaFoundationCdm;
  }
  if (sandbox_string == kProxyResolverSandbox) {
    return Sandbox::kProxyResolver;
  }
#endif
#if BUILDFLAG(IS_MAC)
  if (sandbox_string == kMirroringSandbox) {
    return Sandbox::kMirroring;
  }
#endif
  if (sandbox_string == kAudioSandbox) {
    return Sandbox::kAudio;
  }
  if (sandbox_string == kSpeechRecognitionSandbox) {
    return Sandbox::kSpeechRecognition;
  }
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  if (sandbox_string == kPrintBackendSandbox) {
    return Sandbox::kPrintBackend;
  }
  if (sandbox_string == kScreenAISandbox) {
    return Sandbox::kScreenAI;
  }
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  if (sandbox_string == kOnDeviceTranslationSandbox) {
    return Sandbox::kOnDeviceTranslation;
  }
#endif
#if BUILDFLAG(IS_FUCHSIA)
  if (sandbox_string == kVideoCaptureSandbox) {
    return Sandbox::kVideoCapture;
  }
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (sandbox_string == kShapeDetectionSandbox) {
    return Sandbox::kShapeDetection;
  }
#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
  if (sandbox_string == kHardwareVideoDecodingSandbox) {
    return Sandbox::kHardwareVideoDecoding;
  }
  if (sandbox_string == kHardwareVideoEncodingSandbox) {
    return Sandbox::kHardwareVideoEncoding;
  }
#endif
#endif
#if BUILDFLAG(IS_CHROMEOS)
  if (sandbox_string == kImeSandbox) {
    return Sandbox::kIme;
  }
  if (sandbox_string == kTtsSandbox) {
    return Sandbox::kTts;
  }
  if (sandbox_string == kNearbySandbox) {
    return Sandbox::kNearby;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  NOTREACHED()
      << "Command line does not provide a valid sandbox configuration: "
      << sandbox_string;
}

}  // namespace sandbox::policy
