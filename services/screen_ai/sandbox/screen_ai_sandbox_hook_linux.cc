// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/sandbox/screen_ai_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "services/screen_ai/public/cpp/utilities.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace screen_ai {

namespace {

NO_SANITIZE("cfi-icall")
void CallPresandboxInitFunction(void* presandbox_init_function) {
  DCHECK(presandbox_init_function);
  typedef void (*PresandboxInitFn)();
  (*reinterpret_cast<PresandboxInitFn>(presandbox_init_function))();
}

}  // namespace

bool ScreenAIPreSandboxHook(base::FilePath binary_path,
                            sandbox::policy::SandboxLinux::Options options) {
  if (binary_path.empty()) {
    VLOG(0) << "Screen AI component binary not found.";
  } else {
    void* screen_ai_library = dlopen(binary_path.value().c_str(),
                                     RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
    // The library is delivered by the component updater or DLC. If it is not
    // available or has loading or syntax problems, we cannot do anything about
    // them here. The requests to the service will fail later as the library
    // does not exist or does not initialize.
    if (screen_ai_library == nullptr) {
      VLOG(0) << dlerror();
      binary_path.clear();
    } else {
      void* presandbox_init = dlsym(screen_ai_library, "PresandboxInit");
      if (presandbox_init == nullptr) {
        VLOG(0) << "PresandboxInit function of Screen AI library not found.";
        binary_path.clear();
      } else {
        VLOG(2) << "Screen AI library loaded pre-sandboxing: " << binary_path;
        CallPresandboxInitFunction(presandbox_init);
      }
    }
  }

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/proc/cpuinfo"),
      BrokerFilePermission::ReadOnly("/proc/meminfo")};

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  permissions.push_back(BrokerFilePermission::ReadOnly("/proc/self/status"));
  permissions.push_back(
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/kernel_max"));
  permissions.push_back(
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/possible"));
  permissions.push_back(
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/present"));
#endif

  instance->StartBrokerProcess(
      MakeBrokerCommandSet({sandbox::syscall_broker::COMMAND_ACCESS,
                            sandbox::syscall_broker::COMMAND_OPEN}),
      permissions, options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace screen_ai
