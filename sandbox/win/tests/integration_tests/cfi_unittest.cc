// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
// ASLR must be enabled for CFG to be enabled.  As ASLR is disabled in debug
// builds, so must be CFG.
#if defined(NDEBUG)

// Make sure Microsoft binaries, that are compiled with CFG enabled, catch
// a hook and throw an exception.
// - If this test fails, the expected CFG exception did NOT happen.  This
//   indicates a build system change that has disabled Chrome process-wide CFG.
TEST(CFGSupportTests, DISABLED_MsIndirectFailure) {
  // CFG is only supported on >= Win8.1 Update 3.
  // Not checking for update, since test infra is updated and it would add
  // a lot of complexity.
  if (base::win::GetVersion() < base::win::Version::WIN8_1)
    return;

  const wchar_t* exe_filename = L"cfi_unittest_exe.exe";
  const wchar_t* sys_dll_test = L"1";

  base::CommandLine cmd_line = base::CommandLine::FromString(exe_filename);
  cmd_line.AppendArgNative(sys_dll_test);

  base::Process proc =
      base::LaunchProcess(cmd_line, base::LaunchOptionsForTest());
  ASSERT_TRUE(proc.IsValid());

  int exit_code = 0;
  if (!proc.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                   &exit_code)) {
    // Timeout while waiting.  Try to cleanup.
    proc.Terminate(1, false);
    ADD_FAILURE();
    return;
  }

  // CFG security check failure.
  ASSERT_EQ(STATUS_STACK_BUFFER_OVERRUN, static_cast<DWORD>(exit_code));
}

#endif  // defined(NDEBUG)
}  // namespace sandbox
