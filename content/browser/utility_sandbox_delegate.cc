// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_sandbox_delegate.h"

#include "base/check.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "services/screen_ai/buildflags/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "sandbox/policy/sandbox_type.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {

UtilitySandboxedProcessLauncherDelegate::
    UtilitySandboxedProcessLauncherDelegate(
        sandbox::mojom::Sandbox sandbox_type,
        const base::EnvironmentMap& env,
        const base::CommandLine& cmd_line)
    :
#if BUILDFLAG(IS_POSIX)
      env_(env),
#endif
      sandbox_type_(sandbox_type),
#if BUILDFLAG(IS_WIN)
      app_container_disabled_(
          GetContentClient()->browser()->IsAppContainerDisabled(sandbox_type)),
#endif
      cmd_line_(cmd_line) {
#if DCHECK_IS_ON()
  bool supported_sandbox_type =
      sandbox_type_ == sandbox::mojom::Sandbox::kNoSandbox ||
#if BUILDFLAG(IS_WIN)
      sandbox_type_ ==
          sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges ||
      sandbox_type_ == sandbox::mojom::Sandbox::kXrCompositing ||
      sandbox_type_ == sandbox::mojom::Sandbox::kPdfConversion ||
      sandbox_type_ == sandbox::mojom::Sandbox::kIconReader ||
      sandbox_type_ == sandbox::mojom::Sandbox::kMediaFoundationCdm ||
      sandbox_type_ == sandbox::mojom::Sandbox::kWindowsSystemProxyResolver ||
#endif
#if BUILDFLAG(IS_MAC)
      sandbox_type_ == sandbox::mojom::Sandbox::kMirroring ||
#endif
      sandbox_type_ == sandbox::mojom::Sandbox::kUtility ||
      sandbox_type_ == sandbox::mojom::Sandbox::kService ||
      sandbox_type_ == sandbox::mojom::Sandbox::kServiceWithJit ||
      sandbox_type_ == sandbox::mojom::Sandbox::kNetwork ||
      sandbox_type_ == sandbox::mojom::Sandbox::kOnDeviceModelExecution ||
      sandbox_type_ == sandbox::mojom::Sandbox::kCdm ||
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandbox_type_ == sandbox::mojom::Sandbox::kPrintBackend ||
#endif
      sandbox_type_ == sandbox::mojom::Sandbox::kPrintCompositor ||
#if BUILDFLAG(ENABLE_PPAPI) && !BUILDFLAG(IS_WIN)
      sandbox_type_ == sandbox::mojom::Sandbox::kPpapi ||
#endif
#if BUILDFLAG(IS_FUCHSIA)
      sandbox_type_ == sandbox::mojom::Sandbox::kVideoCapture ||
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::mojom::Sandbox::kHardwareVideoDecoding ||
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      sandbox_type_ == sandbox::mojom::Sandbox::kHardwareVideoEncoding ||
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::mojom::Sandbox::kIme ||
      sandbox_type_ == sandbox::mojom::Sandbox::kTts ||
      sandbox_type_ == sandbox::mojom::Sandbox::kNearby ||
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      sandbox_type_ == sandbox::mojom::Sandbox::kLibassistant ||
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      sandbox_type_ == sandbox::mojom::Sandbox::kScreenAI ||
#endif
#if BUILDFLAG(IS_LINUX)
      sandbox_type_ == sandbox::mojom::Sandbox::kVideoEffects ||
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
      sandbox_type_ == sandbox::mojom::Sandbox::kOnDeviceTranslation ||
#endif
      sandbox_type_ == sandbox::mojom::Sandbox::kAudio ||
      sandbox_type_ == sandbox::mojom::Sandbox::kSpeechRecognition;
  DCHECK(supported_sandbox_type);
#endif  // DCHECK_IS_ON()
}

UtilitySandboxedProcessLauncherDelegate::
    ~UtilitySandboxedProcessLauncherDelegate() = default;

sandbox::mojom::Sandbox
UtilitySandboxedProcessLauncherDelegate::GetSandboxType() {
  return sandbox_type_;
}

#if BUILDFLAG(IS_POSIX)
base::EnvironmentMap UtilitySandboxedProcessLauncherDelegate::GetEnvironment() {
  return env_;
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(USE_ZYGOTE)
ZygoteCommunication* UtilitySandboxedProcessLauncherDelegate::GetZygote() {
  if (zygote_.has_value()) {
    return zygote_.value();
  }

  // If the sandbox has been disabled for a given type, don't use a zygote.
  if (sandbox::policy::IsUnsandboxedSandboxType(sandbox_type_))
    return nullptr;

  // TODO(crbug.com/40261714): remove this special case and fork from the
  // zygote. For now, browser tests fail when forking the network service from
  // the unsandboxed zygote, as the forked process only creates the
  // NetworkServiceTestHelper if the process is exec'd.
  if (sandbox_type_ == sandbox::mojom::Sandbox::kNetwork) {
    return nullptr;
  }

  // Utility processes which need specialized sandboxes fork from the
  // unsandboxed zygote and then apply their actual sandboxes in the forked
  // process upon startup.
  if (sandbox_type_ == sandbox::mojom::Sandbox::kNetwork ||
      sandbox_type_ == sandbox::mojom::Sandbox::kOnDeviceModelExecution ||
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::mojom::Sandbox::kHardwareVideoDecoding ||
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      sandbox_type_ == sandbox::mojom::Sandbox::kHardwareVideoEncoding ||
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::mojom::Sandbox::kIme ||
      sandbox_type_ == sandbox::mojom::Sandbox::kTts ||
      sandbox_type_ == sandbox::mojom::Sandbox::kNearby ||
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      sandbox_type_ == sandbox::mojom::Sandbox::kLibassistant ||
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::mojom::Sandbox::kAudio ||
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandbox_type_ == sandbox::mojom::Sandbox::kPrintBackend ||
#endif
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
      sandbox_type_ == sandbox::mojom::Sandbox::kScreenAI ||
#endif
#if BUILDFLAG(IS_LINUX)
      sandbox_type_ == sandbox::mojom::Sandbox::kVideoEffects ||
      sandbox_type_ == sandbox::mojom::Sandbox::kOnDeviceTranslation ||
#endif  // BUILDFLAG(IS_LINUX)
      sandbox_type_ == sandbox::mojom::Sandbox::kSpeechRecognition) {
    return GetUnsandboxedZygote();
  }

  // All other types use the pre-sandboxed zygote.
  return GetGenericZygote();
}

void UtilitySandboxedProcessLauncherDelegate::SetZygote(
    ZygoteCommunication* handle) {
  zygote_ = handle;
}
#endif  // BUILDFLAG(USE_ZYGOTE)

}  // namespace content
