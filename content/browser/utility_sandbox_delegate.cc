// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_sandbox_delegate.h"

#include "base/check.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/sandbox_type.h"

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/common/zygote/zygote_handle_impl_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {

UtilitySandboxedProcessLauncherDelegate::
    UtilitySandboxedProcessLauncherDelegate(
        sandbox::policy::SandboxType sandbox_type,
        const base::EnvironmentMap& env,
        const base::CommandLine& cmd_line)
    :
#if defined(OS_POSIX)
      env_(env),
#endif
      sandbox_type_(sandbox_type),
      cmd_line_(cmd_line) {
#if DCHECK_IS_ON()
  bool supported_sandbox_type =
      sandbox_type_ == sandbox::policy::SandboxType::kNoSandbox ||
#if defined(OS_WIN)
      sandbox_type_ ==
          sandbox::policy::SandboxType::kNoSandboxAndElevatedPrivileges ||
      sandbox_type_ == sandbox::policy::SandboxType::kXrCompositing ||
      sandbox_type_ == sandbox::policy::SandboxType::kProxyResolver ||
      sandbox_type_ == sandbox::policy::SandboxType::kPdfConversion ||
      sandbox_type_ == sandbox::policy::SandboxType::kIconReader ||
      sandbox_type_ == sandbox::policy::SandboxType::kMediaFoundationCdm ||
#endif
#if defined(OS_MAC)
      sandbox_type_ == sandbox::policy::SandboxType::kMirroring ||
#endif
      sandbox_type_ == sandbox::policy::SandboxType::kUtility ||
      sandbox_type_ == sandbox::policy::SandboxType::kNetwork ||
      sandbox_type_ == sandbox::policy::SandboxType::kCdm ||
#if BUILDFLAG(ENABLE_PRINTING)
      sandbox_type_ == sandbox::policy::SandboxType::kPrintBackend ||
#endif
      sandbox_type_ == sandbox::policy::SandboxType::kPrintCompositor ||
      sandbox_type_ == sandbox::policy::SandboxType::kPpapi ||
      sandbox_type_ == sandbox::policy::SandboxType::kVideoCapture ||
#if BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::policy::SandboxType::kIme ||
      sandbox_type_ == sandbox::policy::SandboxType::kTts ||
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      sandbox_type_ == sandbox::policy::SandboxType::kLibassistant ||
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::policy::SandboxType::kAudio ||
#if !defined(OS_MAC)
      sandbox_type_ == sandbox::policy::SandboxType::kService ||
#endif
      sandbox_type_ == sandbox::policy::SandboxType::kSpeechRecognition;
  DCHECK(supported_sandbox_type);
#endif  // DCHECK_IS_ON()
}

UtilitySandboxedProcessLauncherDelegate::
    ~UtilitySandboxedProcessLauncherDelegate() {}

sandbox::policy::SandboxType
UtilitySandboxedProcessLauncherDelegate::GetSandboxType() {
  return sandbox_type_;
}

#if defined(OS_POSIX)
base::EnvironmentMap UtilitySandboxedProcessLauncherDelegate::GetEnvironment() {
  return env_;
}
#endif  // OS_POSIX

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
ZygoteHandle UtilitySandboxedProcessLauncherDelegate::GetZygote() {
  // If the sandbox has been disabled for a given type, don't use a zygote.
  if (sandbox::policy::IsUnsandboxedSandboxType(sandbox_type_))
    return nullptr;

  // Utility processes which need specialized sandboxes fork from the
  // unsandboxed zygote and then apply their actual sandboxes in the forked
  // process upon startup.
  if (sandbox_type_ == sandbox::policy::SandboxType::kNetwork ||
#if BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::policy::SandboxType::kIme ||
      sandbox_type_ == sandbox::policy::SandboxType::kTts ||
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      sandbox_type_ == sandbox::policy::SandboxType::kLibassistant ||
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      sandbox_type_ == sandbox::policy::SandboxType::kAudio ||
#if BUILDFLAG(ENABLE_PRINTING)
      sandbox_type_ == sandbox::policy::SandboxType::kPrintBackend ||
#endif
      sandbox_type_ == sandbox::policy::SandboxType::kSpeechRecognition) {
    return GetUnsandboxedZygote();
  }

  // All other types use the pre-sandboxed zygote.
  return GetGenericZygote();
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

}  // namespace content
