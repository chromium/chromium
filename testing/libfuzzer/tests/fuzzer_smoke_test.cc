// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/sanitizer_buildflags.h"
#include "base/test/spin_wait.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/tests/fuzz_target.h"

namespace fuzzing {
namespace {

using testing::ContainsRegex;
using testing::ElementsAre;
using testing::IsEmpty;

TEST(FuzzerSmokeTest, EmptyFuzzerFindsNoCrashes) {
  auto target = FuzzTarget::Make("empty_fuzzer");
  ASSERT_TRUE(target);

  EXPECT_TRUE(target->Fuzz({.timeout_secs = 5})) << target->output();

  EXPECT_THAT(target->GetCrashingInputs(), IsEmpty());
}

// TODO(https://crbug.com/445826636): Fix and re-enable.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_UBSAN) || BUILDFLAG(IS_UBSAN_SECURITY)
#define MAYBE_FuzzerSolvesStringComparison DISABLED_FuzzerSolvesStringComparison
#else
#define MAYBE_FuzzerSolvesStringComparison FuzzerSolvesStringComparison
#endif
TEST(FuzzerSmokeTest, MAYBE_FuzzerSolvesStringComparison) {
  auto target = FuzzTarget::Make("string_compare_fuzzer");
  ASSERT_TRUE(target);

  target->Fuzz({.timeout_secs = 5});

  EXPECT_THAT(target->GetCrashingInputs(), ElementsAre("fish"))
      << target->output();
}

// TODO(https://crbug.com/445826636): Fix and re-enable.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_UBSAN) || BUILDFLAG(IS_UBSAN_SECURITY)
#define MAYBE_FuzzerSolvesProtoStringComparison \
  DISABLED_FuzzerSolvesProtoStringComparison
#else
#define MAYBE_FuzzerSolvesProtoStringComparison \
  FuzzerSolvesProtoStringComparison
#endif
TEST(FuzzerSmokeTest, MAYBE_FuzzerSolvesProtoStringComparison) {
  auto target = FuzzTarget::Make("string_compare_proto_fuzzer");
  ASSERT_TRUE(target);

  target->Fuzz({.timeout_secs = 5});

  EXPECT_THAT(target->GetCrashingInputs(), ElementsAre("\012\004fish"))
      << target->output();
}

#if defined(BUILD_LPM_EMPTY_FUZZER)
// TODO(https://crbug.com/526656114): Fix when MSAN builds are fixed.
#if defined(MEMORY_SANITIZER)
#define MAYBE_LpmEmptyFuzzerDoesNotCrashOnStartup \
  DISABLED_LpmEmptyFuzzerDoesNotCrashOnStartup
#else
#define MAYBE_LpmEmptyFuzzerDoesNotCrashOnStartup \
  LpmEmptyFuzzerDoesNotCrashOnStartup
#endif
TEST(FuzzerSmokeTest, MAYBE_LpmEmptyFuzzerDoesNotCrashOnStartup) {
  auto target = FuzzTarget::Make("lpm_empty_fuzzer");
  ASSERT_TRUE(target);

  EXPECT_TRUE(target->Fuzz({.timeout_secs = 2})) << target->output();
}
#endif  // defined(BUILD_LPM_EMPTY_FUZZER)

// This test is limited to POSIX the process leak bug only affects POSIX
// platforms where ClusterFuzz runs with terminate_before_kill=True
// (which uses SIGTERM first).
#if !BUILDFLAG(IS_WIN) && defined(USING_FUZZTEST_WRAPPER)
// TODO(https://crbug.com/536875721): Re-enable when MSAN builds are fixed.
#if defined(MEMORY_SANITIZER)
#define MAYBE_WrapperDoesNotLeakChildOnSIGTERM \
  DISABLED_WrapperDoesNotLeakChildOnSIGTERM
#else
#define MAYBE_WrapperDoesNotLeakChildOnSIGTERM WrapperDoesNotLeakChildOnSIGTERM
#endif
TEST(FuzzerSmokeTest, MAYBE_WrapperDoesNotLeakChildOnSIGTERM) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));

  base::FilePath wrapper_path =
      exe_path.AppendASCII("stub_fuzztest_StubFuzzer_Stub_fuzzer");
  ASSERT_TRUE(base::PathExists(wrapper_path))
      << "Wrapper binary missing: " << wrapper_path.value();

  base::CommandLine cmd(wrapper_path);
  base::LaunchOptions options;
  options.wait = false;
  options.new_process_group = true;

  base::Process wrapper_process = base::LaunchProcess(cmd, options);
  ASSERT_TRUE(wrapper_process.IsValid());

  // Wait for the child process to spawn.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(::TestTimeouts::action_timeout(), [&]() {
    base::ProcessIterator iter(nullptr);
    while (const base::ProcessEntry* entry = iter.NextProcessEntry()) {
      if (entry->parent_pid() == wrapper_process.Pid()) {
        return true;
      }
    }
    return false;
  }());

  base::ProcessId wrapper_pid = wrapper_process.Pid();

  // Verify the process group exists.
  ASSERT_EQ(0, kill(-wrapper_pid, 0));

  // Terminate the wrapper with SIGTERM.
  EXPECT_TRUE(wrapper_process.Terminate(0, false));

  // Wait for the wrapper process to exit.
  int exit_code = 0;
  EXPECT_TRUE(wrapper_process.WaitForExitWithTimeout(
      ::TestTimeouts::action_timeout(), &exit_code));

  // Verify the process group is gone (meaning the child was also killed).
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(
      ::TestTimeouts::action_timeout(),
      kill(-wrapper_pid, 0) != 0 && errno == ESRCH);
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace fuzzing
