// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/switches.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#endif

namespace sandbox {
namespace policy {
namespace switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
const char kServiceSandboxType[] = "service-sandbox-type";

// Must be in sync with "sandbox_type" values as used in service manager's
// manifest.json catalog files.
const char kNoneSandbox[] = "none";
const char kNoneSandboxAndElevatedPrivileges[] = "none_and_elevated";
const char kNetworkSandbox[] = "network";
const char kOnDeviceModelExecutionSandbox[] = "on_device_model_execution";
const char kPpapiSandbox[] = "ppapi";
const char kUtilitySandbox[] = "utility";
const char kCdmSandbox[] = "cdm";
#if BUILDFLAG(ENABLE_PRINTING)
const char kPrintBackendSandbox[] = "print_backend";
#endif
const char kPrintCompositorSandbox[] = "print_compositor";
const char kAudioSandbox[] = "audio";
const char kServiceSandbox[] = "service";
const char kServiceSandboxWithJit[] = "service_with_jit";
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
const char kScreenAISandbox[] = "screen_ai";
#endif
const char kVideoEffectsSandbox[] = "video_effects";
const char kSpeechRecognitionSandbox[] = "speech_recognition";
const char kVideoCaptureSandbox[] = "video_capture";

#if BUILDFLAG(IS_WIN)
const char kPdfConversionSandbox[] = "pdf_conversion";
const char kXrCompositingSandbox[] = "xr_compositing";
const char kIconReaderSandbox[] = "icon_reader";
const char kMediaFoundationCdmSandbox[] = "mf_cdm";
const char kWindowsSystemProxyResolverSandbox[] = "proxy_resolver_win";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
const char kMirroringSandbox[] = "mirroring";
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
const char kHardwareVideoDecodingSandbox[] = "hardware_video_decoding";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const char kHardwareVideoEncodingSandbox[] = "hardware_video_encoding";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kImeSandbox[] = "ime";
const char kTtsSandbox[] = "tts";
const char kNearbySandbox[] = "nearby";
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
const char kLibassistantSandbox[] = "libassistant";
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
const char kOnDeviceTranslationSandbox[] = "on_device_translation";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

// Flags owned by the service manager sandbox.

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

// Add additional capabilities to the AppContainer sandbox on the GPU process.
const char kAddGpuAppContainerCaps[] = "add-gpu-appcontainer-caps";

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

}  // namespace switches
}  // namespace policy
}  // namespace sandbox
