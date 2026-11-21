// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"

#include <fcntl.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

namespace {

using SetResultFn = void (*)(bool);
using GetCallCountFn = int (*)();

DISABLE_CFI_DLSYM void CallSetResult(void* fn, bool result) {
  reinterpret_cast<SetResultFn>(fn)(result);
}

DISABLE_CFI_DLSYM int CallGetCallCount(void* fn) {
  return reinterpret_cast<GetCallCountFn>(fn)();
}

// Loads a test shared library built next to the test executable.
base::ScopedNativeLibrary LoadTestLibrary(std::string_view name) {
  base::FilePath exe_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_dir));
  return base::ScopedNativeLibrary(
      exe_dir.AppendASCII(base::GetNativeLibraryName(name)));
}

bool AllowsOpen(const std::vector<
                    sandbox::syscall_broker::BrokerFilePermission>& permissions,
                const char* path,
                int flags) {
  return std::ranges::any_of(permissions, [&](const auto& permission) {
    return permission.CheckOpen(path, flags).first != nullptr;
  });
}

}  // namespace

TEST(SpeechRecognitionSandboxHookLinuxTest, PermissionsWithoutProcCpuinfo) {
  const auto permissions = GetSodaFilePermissions(/*allow_proc_cpuinfo=*/false);
  EXPECT_TRUE(AllowsOpen(permissions, "/dev/urandom", O_RDONLY));
  EXPECT_TRUE(
      AllowsOpen(permissions, "/sys/devices/system/cpu/possible", O_RDONLY));
  EXPECT_FALSE(
      AllowsOpen(permissions, "/sys/devices/system/cpu/possible", O_WRONLY));
  EXPECT_FALSE(AllowsOpen(permissions, "/proc/cpuinfo", O_RDONLY));
}

TEST(SpeechRecognitionSandboxHookLinuxTest, PermissionsWithProcCpuinfo) {
  const auto permissions = GetSodaFilePermissions(/*allow_proc_cpuinfo=*/true);
  EXPECT_TRUE(AllowsOpen(permissions, "/dev/urandom", O_RDONLY));
  EXPECT_TRUE(
      AllowsOpen(permissions, "/sys/devices/system/cpu/possible", O_RDONLY));
  EXPECT_TRUE(AllowsOpen(permissions, "/proc/cpuinfo", O_RDONLY));
  EXPECT_FALSE(AllowsOpen(permissions, "/proc/cpuinfo", O_WRONLY));
}

TEST(SpeechRecognitionSandboxHookLinuxTest, TryPreSandboxInitNullLibrary) {
  EXPECT_FALSE(TrySodaPreSandboxInit(nullptr));
}

TEST(SpeechRecognitionSandboxHookLinuxTest, TryPreSandboxInitMissingSymbol) {
  base::ScopedNativeLibrary lib =
      LoadTestLibrary("fake_soda_legacy_library_for_testing");
  ASSERT_TRUE(lib.is_valid()) << lib.GetError()->ToString();
  EXPECT_FALSE(TrySodaPreSandboxInit(lib.get()));
}

TEST(SpeechRecognitionSandboxHookLinuxTest, TryPreSandboxInitCallsExport) {
  base::ScopedNativeLibrary lib =
      LoadTestLibrary("fake_soda_presandbox_library_for_testing");
  ASSERT_TRUE(lib.is_valid()) << lib.GetError()->ToString();

  void* set_result = lib.GetFunctionPointer("SetFakeSodaPreSandboxInitResult");
  void* get_call_count =
      lib.GetFunctionPointer("GetFakeSodaPreSandboxInitCallCount");
  ASSERT_TRUE(set_result);
  ASSERT_TRUE(get_call_count);

  const int baseline = CallGetCallCount(get_call_count);

  CallSetResult(set_result, true);
  EXPECT_TRUE(TrySodaPreSandboxInit(lib.get()));
  EXPECT_EQ(CallGetCallCount(get_call_count), baseline + 1);

  CallSetResult(set_result, false);
  EXPECT_FALSE(TrySodaPreSandboxInit(lib.get()));
  EXPECT_EQ(CallGetCallCount(get_call_count), baseline + 2);
}

}  // namespace speech
