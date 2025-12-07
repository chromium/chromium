// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <windows.h>

#include <intrin.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// ASLR must be enabled for CFG to be enabled, and ASLR is disabled in debug
// builds.
// Test cannot run with ASAN builds as CFI linker is currently disabled because
// of perf issues caused by https://crbug.com/846966.
#if !defined(_DEBUG) && !defined(ADDRESS_SANITIZER)

namespace {

#if defined(ARCH_CPU_X86_FAMILY)
// On x86/x64 systems, nop instructions are generally 1 byte.
static constexpr int kNopInstructionSize = 1;
#elif defined(ARCH_CPU_ARM64)
// On Arm systems, all instructions are 4 bytes, fixed size.
static constexpr int kNopInstructionSize = 4;
#else
#error "Unsupported architecture"
#endif

DWORD CALLBACK CopyProgressRoutine(LARGE_INTEGER total_file_size,
                                   LARGE_INTEGER total_bytes_transferred,
                                   LARGE_INTEGER stream_size,
                                   LARGE_INTEGER stream_bytes_transferred,
                                   DWORD stream_number,
                                   DWORD callback_reason,
                                   HANDLE source_file,
                                   HANDLE destination_file,
                                   LPVOID context) {
  asm("nop\n"
      "nop\n"
      "ret\n");
  return PROGRESS_CONTINUE;
}

}  // namespace

static jmp_buf buf;

__declspec(noinline) void PerformLongJump() {
  // Inlining is explicitly disabled for this function because it
  // would eliminate CFG protections.
  longjmp(buf, 1);
}

// Windows on Arm is affected by an LLD code-generation defect around longjmp.
// This regression test checks that using setjmp/longjmp with CFG doesn't
// crash the browser (libjpeg-turbo uses this pattern for error reporting).
TEST(CFGSupportTests, LongJmp) {
  // Initially, setjmp returns zero indicating that the PC etc has been saved in
  // buf.
  if (setjmp(buf)) {
    // Test passes if execution flow reaches here.
    EXPECT_TRUE(true);
    return;
  }
  // Call another function to perform the longjmp.
  PerformLongJump();
  NOTREACHED();
}

// Make sure Microsoft binaries compiled with CFG cannot call indirect pointers
// not listed in the loader config for this test binary.
TEST(CFGSupportTests, MsIndirectFailure) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe_path));

  using ProcessCallbackRoutineType = decltype(&CopyProgressRoutine);

  // Create a bad callback pointer to midway into the callback function. This
  // should cause a CFG violation in MS code.
  auto bad_callback_func = reinterpret_cast<ProcessCallbackRoutineType>(
      (reinterpret_cast<uintptr_t>(CopyProgressRoutine)) + kNopInstructionSize);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file_path = temp_dir.GetPath().AppendASCII("file.dat");
  EXPECT_EXIT(
      // CopyFileEx calls back into our code.
      CopyFileExW(exe_path.value().c_str(), temp_file_path.value().c_str(),
                  bad_callback_func, nullptr, FALSE, 0),
      ::testing::ExitedWithCode(STATUS_STACK_BUFFER_OVERRUN), "");
}

#endif  // !defined(_DEBUG) && !defined(ADDRESS_SANITIZER)

}  // namespace sandbox
