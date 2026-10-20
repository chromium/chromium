// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/sanitizer_buildflags.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/libfuzzer/buildflags.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

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

// Regexes in this file are intentionally loose: each one describes a single
// semantically meaningful line of output (e.g. "a stack frame for
// `TriggerCheck`") rather than the exact formatting of a particular
// sanitizer, symbolizer or platform. They are matched one at a time, in order,
// by `ContainsInOrder()` and any amount of output may appear between them.
//
// RE2 compiles regexes without "dotall" semantics, so '.' does not match
// newlines and ".*" matches within a single line at most.

// A hexadecimal address, as printed in stack frames.
#define HEX_ADDRESS "0x[0-9a-fA-F]+"

// The CHECK failure log line. Matches both the default log format:
//   [<prefix>:FATAL:<path>/stacktrace_test_fuzzer.cc:24] Check failed: ...
// and the ChromeOS syslog format:
//   <prefix> FATAL <binary>: [<path>/stacktrace_test_fuzzer.cc:24] Check ...
//
// If the `<file>:<line>] Check failed:` part of the format changes, then
// ClusterFuzz's regex should be adjusted to match. See e.g.
// https://crbug.com/443678564.
constexpr std::string_view kCheckFailedLineRegex =
    R"(FATAL[: ].*\bstacktrace_test_fuzzer\.cc:\d+\] Check failed: false)";

// A symbolized stack frame for `TriggerCheck()`, in any of the formats printed
// by the sanitizers, libFuzzer, Centipede or base::debug::StackTrace. All that
// is required is an address and the function name on the same line. No word
// boundaries are required around the name so that mangled names (e.g.
// `_Z12TriggerCheckv`) also match.
constexpr std::string_view kTriggerCheckFrameRegex =
    HEX_ADDRESS R"(.*TriggerCheck|TriggerCheck.*)" HEX_ADDRESS;

// Any numbered stack frame, symbolized or not, as printed by the sanitizers
// and base::debug::StackTrace on Linux and ChromeOS, e.g.:
//   #0 0x55d1e0c1a2b3 <unknown>
//   #8 0x5636df5a72d2 in TriggerCheck() <path>/stacktrace_test_fuzzer.cc:24:3
constexpr std::string_view kAnyStackFrameRegex = R"(#\d+ )" HEX_ADDRESS;

// The heap-use-after-free report header and access line printed by ASan.
constexpr std::string_view kAsanUafHeaderRegex =
    R"(ERROR: AddressSanitizer: heap-use-after-free on address )" HEX_ADDRESS;
constexpr std::string_view kAsanUafAccessRegex =
    R"(READ of size \d+ at )" HEX_ADDRESS;

// The top stack frame of the UaF report, which must be `TriggerUAF()`. Windows
// prints `TriggerUAF(void)`, other platforms print `TriggerUAF()`.
constexpr std::string_view kTriggerUafTopFrameRegex =
    R"(#0 )" HEX_ADDRESS R"( in TriggerUAF\b)";

// Same as above, but also requires the frame to include a source location.
constexpr std::string_view kTriggerUafTopFrameWithSourceRegex =
    R"(#0 )" HEX_ADDRESS
    R"( in TriggerUAF\b.*\bstacktrace_test_fuzzer\.cc:\d+)";

#undef HEX_ADDRESS

// Whether the CHECK failure output is guaranteed to contain a symbolized
// `TriggerCheck()` frame. If not, the test only checks that a stack trace is
// printed after the CHECK failure.
//
// On these configurations the in-process stack trace printed by base is not
// symbolized, while whether libFuzzer's own trace symbolizes `TriggerCheck()`
// depends on the exact build config (e.g. it does in component builds). It
// used to be asserted that `TriggerCheck` was *absent* here, which was a
// source of flakiness across builders.
//
// TODO(https://crbug.com/40948553): Symbolize CHECK failures on ChromeOS and
// Linux ASan builds.
#if BUILDFLAG(IS_CHROMEOS) || \
    (BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER))
constexpr bool kExpectSymbolizedCheckFailure = false;
#else
constexpr bool kExpectSymbolizedCheckFailure = true;
#endif

// Checks that `output` contains a match for each of `regexes`, in order. Each
// match must start after the end of the previous one. On failure, reports the
// first regex that could not be matched, which makes failures much easier to
// diagnose than a single monolithic regex.
testing::AssertionResult ContainsInOrder(
    std::string_view output,
    std::initializer_list<std::string_view> regexes) {
  std::string_view remaining = output;
  for (std::string_view regex : regexes) {
    // Wrap the regex in a group so that top-level alternations are scoped
    // correctly.
    re2::RE2 re(base::StrCat({"(?:", regex, ")"}));
    if (!re.ok()) {
      return testing::AssertionFailure()
             << "Invalid regex `" << regex << "`: " << re.error();
    }
    // Advances `remaining` past the end of the match.
    if (!re2::RE2::FindAndConsume(&remaining, re)) {
      return testing::AssertionFailure()
             << "No match for regex `" << regex << "` in output after offset "
             << (output.size() - remaining.size());
    }
  }
  return testing::AssertionSuccess();
}

// All regexes are compiled in every build configuration, so that errors in
// regexes only used on other platforms surface quickly during local
// development.
TEST(FuzzerStacktraceTest, RegexesAreValid) {
  for (std::string_view regex : {
           kCheckFailedLineRegex,
           kTriggerCheckFrameRegex,
           kAnyStackFrameRegex,
           kAsanUafHeaderRegex,
           kAsanUafAccessRegex,
           kTriggerUafTopFrameRegex,
           kTriggerUafTopFrameWithSourceRegex,
       }) {
    EXPECT_EQ(re2::RE2(regex).error(), "") << regex;
  }
}

// The UaF is not detected under UBSan, which happily runs the fuzzer forever.
#if !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)

TEST(FuzzerStacktraceTest, SymbolizesUAF) {
  base::CommandLine cmd(FuzzerPath());
  cmd.AppendArgPath(FuzzerInputPath("uaf"));

  // This loosely replicates how we run fuzzers in production. We let ASAN
  // handle the symbolization online - which fails for sandboxed processes, but
  // this toy fuzzer does not run any sandboxed code.
  base::ScopedEnvironmentVariableOverride asan_options("ASAN_OPTIONS",
                                                       "symbolize=1");

  std::string output;
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));  // Target crashes.

// TODO(https://crbug.com/40948553): Get MSan fuzzer build to work and expect
// the correct output here.
#if defined(ADDRESS_SANITIZER)

// Whether ASan stack frames include a source file and line. Mac is currently
// the only platform that doesn't reliably have these.
#if BUILDFLAG(IS_MAC)
  constexpr bool kExpectAsanSourceLocations = false;
#else
  constexpr bool kExpectAsanSourceLocations = true;
#endif

  EXPECT_TRUE(ContainsInOrder(
      output,
      {
          kAsanUafHeaderRegex,
          kAsanUafAccessRegex,
          kExpectAsanSourceLocations ? kTriggerUafTopFrameWithSourceRegex
                                     : kTriggerUafTopFrameRegex,
      }))
      << output;  // Print unescaped stack trace for easier debugging.

#endif  // defined(ADDRESS_SANITIZER)
}

#endif  // !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)

// TODO(https://crbug.com/575052824): Re-enable this once MSan builds are
// properly supported.
#if defined(MEMORY_SANITIZER)
#define MAYBE_SymbolizesCheck DISABLED_SymbolizesCheck
#else
#define MAYBE_SymbolizesCheck SymbolizesCheck
#endif
TEST(FuzzerStacktraceTest, MAYBE_SymbolizesCheck) {
  base::CommandLine cmd(FuzzerPath());
  cmd.AppendArgPath(FuzzerInputPath("check"));

  base::ScopedEnvironmentVariableOverride asan_options("ASAN_OPTIONS",
                                                       "symbolize=1");

  std::string output;
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));

  if (kExpectSymbolizedCheckFailure) {
    EXPECT_TRUE(ContainsInOrder(output,
                                {
                                    kCheckFailedLineRegex,
                                    kTriggerCheckFrameRegex,
                                }))
        << output;
  } else {
    EXPECT_TRUE(ContainsInOrder(output,
                                {
                                    kCheckFailedLineRegex,
                                    kAnyStackFrameRegex,
                                }))
        << output;
  }
}

}  // namespace
