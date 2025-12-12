// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/buildflags.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

base::FilePath FuzzerPath() {
  base::FilePath out_dir;
  base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &out_dir);

  return out_dir.AppendASCII("stacktrace_test_fuzzer");
}

base::FilePath FuzzerInputPath(std::string_view input_file) {
  base::FilePath src_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir);

  return src_dir.AppendASCII("testing/libfuzzer/tests/data")
      .AppendASCII(input_file);
}

// The UaF is not detected under UBSan, which happily runs the fuzzer forever.
#if !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)

TEST(FuzzerStacktraceTest, SymbolizesUAF) {
  base::CommandLine cmd(FuzzerPath());
  cmd.AppendArgPath(FuzzerInputPath("uaf"));

  // This loosely replicates how we run fuzzers in production. Wet let ASAN
  // handle the symbolization online - which fails for sandboxed processes, but
  // this toy fuzzer does not run any sandboxed code.
  auto environment = base::Environment::Create();
  environment->SetVar("ASAN_OPTIONS", "symbolize=1");

  std::string output;
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));  // Target crashes.

// TODO(https://crbug.com/40948553): Get MSan fuzzer build to work and expect
// the correct output here.
#if defined(ADDRESS_SANITIZER)

  constexpr std::array<std::string_view, 3> kRegexLines = {
      R"(ERROR: AddressSanitizer: heap-use-after-free on address 0x[0-9a-f]+.*)",
      R"(READ of size 4 at 0x[0-9a-f]+ thread T[0-9]+)",
#if BUILDFLAG(IS_WIN)
      R"(#0 0x[0-9a-f]+ in TriggerUAF [A-Z]:\\.*testing\\libfuzzer\\tests\\stacktrace_test_fuzzer.cc:[0-9]+)",
#elif BUILDFLAG(IS_MAC)
      R"(#0 0x[0-9a-f]+ in TriggerUAF\(\) \(.*/stacktrace_test_fuzzer:arm64\+0x[0-9a-f]+\))",
#else
      R"(#0 0x[0-9a-f]+ in TriggerUAF\(\) testing/libfuzzer/tests/stacktrace_test_fuzzer.cc:[0-9]+:[0-9]+)",
#endif
  };

  EXPECT_THAT(output, ContainsRegex(base::JoinString(kRegexLines, "\n *")))
      << output;  // Print unescaped stack trace for easier debugging.

#endif  // defined(ADDRESS_SANITIZER)
}

#endif  // !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)

// RE2 and thus GTest compile regexes without "dotall" semantics, so '.' does
// not match newlines and ".*"  matches a single line at most. This regex
// can be used instead to match any number of lines.
constexpr std::string_view kSkipLines = R"([\s\S]*)";

// If the format of the check failure changes, then ClusterFuzz's regex
// should be adjusted to match. See e.g. https://crbug.com/443678564.
constexpr std::string_view kCheckFailedLineRegex =
    R"(\[[^:]*:FATAL:[^:]*[/\\]stacktrace_test_fuzzer.cc:24\] Check failed: false. *)";

// ChromeOS has a different prefix for its logs.
constexpr std::string_view kCheckFailedLineRegexChromeOs =
    R"(.* FATAL stacktrace_test_fuzzer: \[testing/libfuzzer/tests/stacktrace_test_fuzzer.cc:24\] Check failed: false. *)";

// Printed by libfuzzer's signal handler.
constexpr std::string_view kLibfuzzerSignalRegex =
    R"(==\d+== ERROR: libFuzzer: deadly signal)";

// Printed by ASan when it cannot or has not symbolized the stack frame.
constexpr std::string_view kUnsymbolizedAsanStackFrameRegex =
    R"(#0 0x[0-9a-f]+ <unknown>)";

// Topmost stack frame as printed by libfuzzer.
constexpr std::string_view kTopLibfuzzerStackFrameRegex =
    R"(#0 0x[0-9a-f]+ in __sanitizer_print_stack_trace .*)";

// `TriggerCheck()` stack frame as printed by libfuzzer.
constexpr std::string_view kTriggerCheckLibfuzzerStackFrameRegex =
    R"(#\d+ 0x[0-9a-f]+ in TriggerCheck\(\) testing/libfuzzer/tests/stacktrace_test_fuzzer\.cc:24:3)";

std::string JoinRegexLines(const std::vector<std::string_view>& lines) {
  return base::JoinString(lines, R"(\r?\n[ \t]*)");
}

std::string CheckFailureStackRegexLinux() {
  return JoinRegexLines({
      kCheckFailedLineRegex,
      kSkipLines,
      R"(#\d+ 0x[0-9a-f]+ TriggerCheck\(\).*)",
      kSkipLines,
      kLibfuzzerSignalRegex,
      kSkipLines,
      kTriggerCheckLibfuzzerStackFrameRegex,
  });
}

std::string CheckFailureStackRegexLinuxAsan64Bit() {
  return JoinRegexLines({
      kCheckFailedLineRegex,
      // TODO(https://crbug.com/40948553): Expect `TriggerCheck` here or in the
      // libfuzzer stack trace below.
      kUnsymbolizedAsanStackFrameRegex,
      kSkipLines,
      kLibfuzzerSignalRegex,
      // This part has symbols, but the stack does not include `TriggerCheck`.
      kTopLibfuzzerStackFrameRegex,
  });
}

std::string CheckFailureStackRegexLinuxCentipedeAsan64Bit() {
  return JoinRegexLines({
      kCheckFailedLineRegex,
      kUnsymbolizedAsanStackFrameRegex,
      kSkipLines,
  });
}

std::string CheckFailureStackRegexLinuxAsan32Bit() {
  return JoinRegexLines({
      kCheckFailedLineRegex,
      kUnsymbolizedAsanStackFrameRegex,
      kSkipLines,
      kLibfuzzerSignalRegex,
      kSkipLines,
      kTriggerCheckLibfuzzerStackFrameRegex,
  });
}

std::string CheckFailureStackRegexChromeOs() {
  return JoinRegexLines({
      kCheckFailedLineRegexChromeOs,
      // TODO(https://crbug.com/40948553): Expect `TriggerCheck` here or in the
      // libfuzzer stack trace below.
      kUnsymbolizedAsanStackFrameRegex,
      kSkipLines,
      kLibfuzzerSignalRegex,
      // This part has symbols, but the stack does not include `TriggerCheck`.
      kTopLibfuzzerStackFrameRegex,
  });
}

std::string CheckFailureStackRegexWin() {
  return JoinRegexLines({
      kCheckFailedLineRegex,
      // TODO(https:/crbug.com/40948553): Expect symbols instead.
      R"(Symbols not available\. Dumping unresolved backtrace:)",
      R"(0x[0-9a-f]+)",
  });
}

std::string CheckFailureStackRegexMacArm64() {
  return JoinRegexLines({
      kCheckFailedLineRegex,
      kSkipLines,
      R"(\d+ +stacktrace_test_fuzzer +0x[0-9a-f]+ .*TriggerCheck.* \+ \d+)",
      kSkipLines,
      kLibfuzzerSignalRegex,
      kSkipLines,
      R"(#\d+ 0x[0-9a-f]+ in TriggerCheck\(\) \(.*/stacktrace_test_fuzzer:arm64\+0x[0-9a-f]+\))",
  });
}

std::string CheckFailureStackRegex() {
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER) && \
    defined(ARCH_CPU_32_BITS)
  return CheckFailureStackRegexLinuxAsan32Bit();
#elif BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_CENTIPEDE) && \
    defined(ADDRESS_SANITIZER)
  return CheckFailureStackRegexLinuxCentipedeAsan64Bit();
#elif BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
  return CheckFailureStackRegexLinuxAsan64Bit();
#elif BUILDFLAG(IS_LINUX)
  return CheckFailureStackRegexLinux();
#elif BUILDFLAG(IS_CHROMEOS)
  return CheckFailureStackRegexChromeOs();
#elif BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  return CheckFailureStackRegexMacArm64();
#elif BUILDFLAG(IS_WIN)
  return CheckFailureStackRegexWin();
#endif
}

// This test mostly exists to silence unused code compiler warnings. It is
// useful to define and compile all regexes in all build configurations, to
// surface any errors quickly during local development builds.
TEST(FuzzerStacktraceTest, CheckFailureRegexesAreValid) {
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexLinux()).error(), "");
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexLinuxAsan32Bit()).error(), "");
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexLinuxAsan64Bit()).error(), "");
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexLinuxCentipedeAsan64Bit()).error(),
            "");
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexChromeOs()).error(), "");
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexMacArm64()).error(), "");
  EXPECT_EQ(re2::RE2(CheckFailureStackRegexWin()).error(), "");
}

// Fuzzer fails to run under MSan.
// TODO(https://crbug.com/326101784): Re-enable this once MSan build is fixed.
#if defined(MEMORY_SANITIZER)
#define MAYBE_SymbolizesCheck DISABLED_SymbolizesCheck
#else
#define MAYBE_SymbolizesCheck SymbolizesCheck
#endif
TEST(FuzzerStacktraceTest, MAYBE_SymbolizesCheck) {
  base::CommandLine cmd(FuzzerPath());
  cmd.AppendArgPath(FuzzerInputPath("check"));

  std::string output;
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));  // Target crashes.

  EXPECT_THAT(output, ContainsRegex(CheckFailureStackRegex()))
      << output;  // Print unescaped stack trace for easier debugging.

  // If the regex does not mention `TriggerCheck`, then check that the stack
  // trace does not contain it either. This checks that the stack trace is not
  // unexpectedly symbolized.
  if (CheckFailureStackRegex().find("TriggerCheck") == std::string::npos) {
    EXPECT_THAT(output, Not(HasSubstr("TriggerCheck"))) << output;
  }
}

}  // namespace
