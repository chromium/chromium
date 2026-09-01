// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <ostream>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/sanitizer_buildflags.h"
#include "base/test/spin_wait.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/buildflags.h"
#include "testing/libfuzzer/tests/fuzz_target.h"

namespace fuzzing {
namespace {

using testing::IsEmpty;
using testing::Not;

enum class FuzzExpectation {
  kSuccess,
  kCrash,
  kExecutionFailure,
};

struct FuzzerTestCase {
  std::string_view fuzzer;
  FuzzExpectation expectation;
};

void PrintTo(const FuzzerTestCase& test_case, std::ostream* os) {
  *os << test_case.fuzzer << " (expectation=";
  switch (test_case.expectation) {
    case FuzzExpectation::kSuccess:
      *os << "kSuccess";
      break;
    case FuzzExpectation::kCrash:
      *os << "kCrash";
      break;
    case FuzzExpectation::kExecutionFailure:
      *os << "kExecutionFailure";
      break;
  }
  *os << ")";
}

constexpr FuzzerTestCase kFuzzerTestCases[] = {
    // LLVM-style Fuzzers
    {
        .fuzzer = "llvm_stub_fuzzer",
        .expectation = FuzzExpectation::kSuccess,
    },
    {
        .fuzzer = "llvm_crashing_fuzzer",
        .expectation = FuzzExpectation::kCrash,
    },
    {
        .fuzzer = "lpm_stub_fuzzer",
        .expectation = FuzzExpectation::kSuccess,
    },

// Native FuzzTest Wrappers
// FuzzTest wrapper executables (*_fuzzer) are only generated on Linux, Mac,
// and Windows (see _building_fuzztest_fuzzer in testing/test.gni).
#if BUILDFLAG(USE_FUZZTEST_WRAPPER) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
    {
        .fuzzer = "fuzztest_stub_fuzzer_FuzzTestStub_Stub_fuzzer",
        .expectation = FuzzExpectation::kSuccess,
    },
    {
        .fuzzer = "fuzztest_crashing_fuzzer_FuzzTestCrashing_FastCrash_fuzzer",
        .expectation = FuzzExpectation::kCrash,
    },
    {
        .fuzzer = "fuzztest_proto_stub_fuzzer_FuzzTestProtoStub_Stub_fuzzer",
        .expectation = FuzzExpectation::kSuccess,
    },
    {
        .fuzzer =
            "fuzztest_proto_crashing_fuzzer_FuzzTestProtoCrashing_FastCrash_"
            "fuzzer",
        .expectation = FuzzExpectation::kCrash,
    },
#endif

// Wrapped LLVM Fuzzers
#if BUILDFLAG(USE_CENTIPEDE) && BUILDFLAG(USE_FUZZTEST_WRAPPER) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
    {
        .fuzzer = "wrapped_llvm_stub_fuzzer_LLVMFuzzer_TestOneInput_fuzzer",
        .expectation = FuzzExpectation::kSuccess,
    },
    {
        .fuzzer = "wrapped_llvm_crashing_fuzzer_LLVMFuzzer_TestOneInput_fuzzer",
        .expectation = FuzzExpectation::kCrash,
    },
#endif
};

class FuzzerSmokeTest : public testing::TestWithParam<FuzzerTestCase> {};

TEST_P(FuzzerSmokeTest, Fuzz) {
  const FuzzerTestCase& test_case = GetParam();
  auto target = FuzzTarget::Make(test_case.fuzzer);
  ASSERT_TRUE(target);

  switch (test_case.expectation) {
    case FuzzExpectation::kSuccess:
      EXPECT_TRUE(target->Fuzz());
      EXPECT_THAT(target->GetCrashingInputs(), IsEmpty());
      break;

    case FuzzExpectation::kCrash:
#if BUILDFLAG(USE_CENTIPEDE)
      EXPECT_TRUE(target->Fuzz());
#else
      EXPECT_FALSE(target->Fuzz());
#endif
      EXPECT_THAT(target->GetCrashingInputs(), Not(IsEmpty()));
      break;

    case FuzzExpectation::kExecutionFailure:
      EXPECT_FALSE(target->Fuzz());
      EXPECT_THAT(target->GetCrashingInputs(), IsEmpty());
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FuzzerSmokeTest,
    testing::ValuesIn(kFuzzerTestCases),
    [](const testing::TestParamInfo<FuzzerTestCase>& info) {
      std::string name(info.param.fuzzer);
      std::replace(name.begin(), name.end(), '.', '_');
      return name;
    });

// This test is limited to POSIX because the process leak bug only affects POSIX
// platforms where ClusterFuzz runs with terminate_before_kill=True
// (which uses SIGTERM first).
#if BUILDFLAG(USE_FUZZTEST_WRAPPER) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC))
TEST(FuzzTestWrapperSmokeTest, WrapperDoesNotLeakChildOnSIGTERM) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));

  base::FilePath wrapper_path =
      exe_path.AppendASCII("fuzztest_stub_fuzzer_FuzzTestStub_Stub_fuzzer");
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
#endif  // BUILDFLAG(USE_FUZZTEST_WRAPPER) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_MAC))

}  // namespace
}  // namespace fuzzing
