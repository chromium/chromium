// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "components/soda/buildflags.h"
#include "components/soda/constants.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace speech {

namespace {

DISABLE_CFI_DLSYM bool CallSodaPreSandboxInit(void* presandbox_init_function) {
  DCHECK(presandbox_init_function);
  using SodaPreSandboxInitFn = bool (*)();
  return reinterpret_cast<SodaPreSandboxInitFn>(presandbox_init_function)();
}

}  // namespace

std::vector<BrokerFilePermission> GetSodaFilePermissions(
    bool allow_proc_cpuinfo) {
  auto soda_dir = GetSodaDirectory();
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      // Read by the memory allocator bundled with libsoda.so when it is loaded
      // after the sandbox has been engaged. Without it the allocator aborts.
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/possible")};

  if (allow_proc_cpuinfo) {
    // Fallback for older SODA binaries without SodaPreSandboxInit (or if
    // pre-sandbox initialization failed): cpuinfo reads /proc/cpuinfo when
    // XNNPACK initializes for the SodaXnnpackDelegate feature.
    // TODO(crbug.com/573909933): Remove once SODA binaries exporting
    // SodaPreSandboxInit have rolled out everywhere.
    permissions.push_back(BrokerFilePermission::ReadOnly("/proc/cpuinfo"));
  }

  // This may happen if a user doesn't have a SODA installation.
  if (!soda_dir.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        soda_dir.AsEndingWithSeparator().value()));
  }

  // This may happen if a user doesn't have any language packs installed.
  auto language_packs_dir = GetSodaLanguagePacksDirectory();
  if (!language_packs_dir.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        language_packs_dir.AsEndingWithSeparator().value()));
  }

#if BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)
  auto test_resources_dir = GetSodaTestResourcesDirectory();
  if (!test_resources_dir.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        test_resources_dir.AsEndingWithSeparator().value()));
  }
#endif

  return permissions;
}

bool TrySodaPreSandboxInit(void* soda_library) {
  if (!soda_library) {
    return false;
  }
  void* presandbox_init = dlsym(soda_library, "SodaPreSandboxInit");
  if (!presandbox_init) {
    VLOG(1) << "SodaPreSandboxInit not found; falling back to brokering "
               "/proc/cpuinfo.";
    return false;
  }
  if (!CallSodaPreSandboxInit(presandbox_init)) {
    LOG(WARNING) << "SodaPreSandboxInit failed; falling back to brokering "
                    "/proc/cpuinfo.";
    return false;
  }
  VLOG(1) << "SodaPreSandboxInit succeeded; omitting /proc/cpuinfo from "
             "sandbox broker.";
  return true;
}

const char* GetSodaBinaryPathSwitch() {
  return kSodaBinaryPathSwitch;
}

bool SpeechRecognitionPreSandboxHook(
    base::FilePath binary_path,
    sandbox::policy::SandboxLinux::Options options) {
#if BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)
  base::FilePath test_binary_path = GetSodaTestBinaryPath();
  DCHECK(base::PathExists(test_binary_path));
  void* soda_test_library = dlopen(GetSodaTestBinaryPath().value().c_str(),
                                   RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_test_library);
  const bool test_presandbox_initialized =
      TrySodaPreSandboxInit(soda_test_library);
#endif

  // Fall back to the latest installed binary for embedders that don't pass the
  // path of the binary that they will ask this process to load.
  if (binary_path.empty()) {
    binary_path = GetSodaBinaryPath();
  }

  void* soda_library = dlopen(binary_path.value().c_str(),
                              RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_library);
  bool presandbox_initialized = TrySodaPreSandboxInit(soda_library);
#if BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)
  presandbox_initialized =
      presandbox_initialized && test_presandbox_initialized;
#endif

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(
      MakeBrokerCommandSet({
          sandbox::syscall_broker::COMMAND_ACCESS,
          sandbox::syscall_broker::COMMAND_OPEN,
          sandbox::syscall_broker::COMMAND_READLINK,
          sandbox::syscall_broker::COMMAND_STAT,
      }),
      GetSodaFilePermissions(/*allow_proc_cpuinfo=*/!presandbox_initialized),
      options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace speech
