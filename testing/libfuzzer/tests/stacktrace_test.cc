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

namespace {

using testing::ContainsRegex;

// The UaF is not detected under UBSan, which happily runs the fuzzer forever.
#if !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)

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

}  // namespace
