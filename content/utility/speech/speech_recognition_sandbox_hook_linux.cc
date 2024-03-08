// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "components/soda/buildflags.h"
#include "components/soda/constants.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace speech {

namespace {

// Gets the file permissions required by the Speech On-Device API (SODA).
std::vector<BrokerFilePermission> GetSodaFilePermissions() {
  auto soda_dir = GetSodaDirectory();
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom")};

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

}  // namespace

bool SpeechRecognitionPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
#if BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)
  base::FilePath test_binary_path = GetSodaTestBinaryPath();
  DVLOG(0) << "SODA test binary path: " << test_binary_path.value().c_str();
  DCHECK(base::PathExists(test_binary_path));
  void* soda_test_library = dlopen(GetSodaTestBinaryPath().value().c_str(),
                                   RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_test_library);
#endif

  void* soda_library = dlopen(GetSodaBinaryPath().value().c_str(),
                              RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_library);

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_READLINK,
                                   sandbox::syscall_broker::COMMAND_STAT,
                               }),
                               GetSodaFilePermissions(), options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace speech
