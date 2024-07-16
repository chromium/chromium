// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ContainsRegex;

// The UaF is not detected under UBSan, which happily runs the fuzzer forever.
#if !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)
TEST(FuzzerStacktraceTest, SymbolizesUAF) {
  base::FilePath exe_path;
  base::PathService::Get(base::FILE_EXE, &exe_path);
  base::FilePath fuzzer_path = exe_path.DirName().Append(
      FILE_PATH_LITERAL("stacktrace_test_uaf_fuzzer"));

  std::string output;
  base::CommandLine cmd(fuzzer_path);
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));  // Target crashes.

// TODO(https://crbug.com/40948553): Get MSan fuzzer build to work and expect
// the correct output here.
#if defined(ADDRESS_SANITIZER)
  constexpr std::string_view kOutput = R"(
ERROR: AddressSanitizer: heap-use-after-free on address 0x[0-9a-f]+.*
READ of size 4 at 0x[0-9a-f]+ thread T[0-9]+
    #0 0x[0-9a-f]+ in TriggerUAF\(\) testing/libfuzzer/tests/stacktrace_test_uaf_fuzzer.cc:[0-9]+:[0-9]+
  )";
  EXPECT_THAT(output, ContainsRegex(
                          base::TrimWhitespaceASCII(kOutput, base::TRIM_ALL)));
#endif
}
#endif  // !BUILDFLAG(IS_UBSAN) && !BUILDFLAG(IS_UBSAN_SECURITY)

}  // namespace
