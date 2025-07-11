// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "sandbox/mac/seatbelt.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "base/check_op.h"
#include "base/process/process_info.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox.h"
#endif  // BUILDFLAG(IS_WIN)

namespace sandbox {
namespace policy {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool Sandbox::Initialize(sandbox::mojom::Sandbox sandbox_type,
                         SandboxLinux::PreSandboxHook hook,
                         const SandboxLinux::Options& options) {
  return SandboxLinux::GetInstance()->InitializeSandbox(
      sandbox_type, std::move(hook), options);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
bool Sandbox::Initialize(sandbox::mojom::Sandbox sandbox_type,
                         SandboxInterfaceInfo* sandbox_info) {
  BrokerServices* broker_services = sandbox_info->broker_services;
  if (broker_services) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (!SandboxWin::InitBrokerServices(broker_services))
      return false;

    // Only pre-create alternate desktop if there will be sandboxed processes in
    // the future.
    if (!command_line.HasSwitch(switches::kNoSandbox)) {
      // IMPORTANT: This piece of code needs to run as early as possible in the
      // process because it will initialize the sandbox broker, which requires
      // the process to swap its window station. During this time all the UI
      // will be broken. This has to run before threads and windows are created.
      ResultCode result = broker_services->CreateAlternateDesktop(
          Desktop::kAlternateWinstation);
      // This failure is usually caused by third-party software or by the host
      // system exhausting its desktop heap.
      CHECK(result == SBOX_ALL_OK);
    }
    return true;
  }
  return IsUnsandboxedSandboxType(sandbox_type) ||
         SandboxWin::InitTargetServices(sandbox_info->target_services);
}
#endif  // BUILDFLAG(IS_WIN)

// static
bool Sandbox::IsProcessSandboxed() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool is_browser = !command_line->HasSwitch(switches::kProcessType);

  if (!is_browser &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoSandbox)) {
    // When running with --no-sandbox, unconditionally report the process as
    // sandboxed. This lets code write |DCHECK(IsProcessSandboxed())| and not
    // break when testing with the --no-sandbox switch.
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  // Note that this does not check the status of the Seccomp sandbox. Call
  // https://developer.android.com/reference/android/os/Process#isIsolated().
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jclass> process_class =
      base::android::GetClass(env, "android/os/Process");
  jmethodID is_isolated =
      base::android::MethodID::Get<base::android::MethodID::TYPE_STATIC>(
          env, process_class.obj(), "isIsolated", "()Z");
  return env->CallStaticBooleanMethod(process_class.obj(), is_isolated);
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40126761): Figure out what to do here. Process
  // launching controls the sandbox and there are no ambient capabilities, so
  // basically everything but the browser is considered sandboxed.
  return !is_browser;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  int status = SandboxLinux::GetInstance()->GetStatus();
  constexpr int kLayer1Flags = SandboxLinux::Status::kSUID |
                               SandboxLinux::Status::kPIDNS |
                               SandboxLinux::Status::kUserNS;
  constexpr int kLayer2Flags =
      SandboxLinux::Status::kSeccompBPF | SandboxLinux::Status::kSeccompTSYNC;
  return (status & kLayer1Flags) != 0 && (status & kLayer2Flags) != 0;
#elif BUILDFLAG(IS_MAC)
  return Seatbelt::IsSandboxed();
#elif BUILDFLAG(IS_WIN)
  return base::GetCurrentProcessIntegrityLevel() < base::MEDIUM_INTEGRITY;
#else
  return false;
#endif
}

}  // namespace policy
}  // namespace sandbox
