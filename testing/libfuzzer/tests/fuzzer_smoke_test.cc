// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/sanitizer_buildflags.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ContainsRegex;

base::FilePath FuzzerPath(std::string_view fuzzer_name) {
  base::FilePath out_dir;
  base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &out_dir);

  return out_dir.AppendASCII(fuzzer_name);
}

TEST(FuzzerSmokeTest, EmptyFuzzerRunsForOneSecond) {
  base::CommandLine cmd(FuzzerPath("empty_fuzzer"));
  cmd.AppendArg("-max_total_time=1");

  std::string output;
  EXPECT_TRUE(base::GetAppOutputAndError(cmd, &output));  // No crash.

  EXPECT_THAT(output,
              ContainsRegex(R"(Done [1-9][0-9]* runs in [0-9]+ second\(s\))"))
      << output;  // Print unescaped stack trace for easier debugging.
}

// TODO(https://crbug.com/445826636): Fix and re-enable.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_UBSAN) || BUILDFLAG(IS_UBSAN_SECURITY)
#define MAYBE_FuzzerSolvesStringComparison DISABLED_FuzzerSolvesStringComparison
#else
#define MAYBE_FuzzerSolvesStringComparison FuzzerSolvesStringComparison
#endif
TEST(FuzzerSmokeTest, MAYBE_FuzzerSolvesStringComparison) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  base::FilePath solution_path = dir.GetPath().AppendASCII("solution");

  base::CommandLine cmd(FuzzerPath("string_compare_fuzzer"));
  cmd.AppendArg("-max_total_time=5");
  cmd.AppendArg("-exact_artifact_path=" + solution_path.MaybeAsASCII());

  std::string output;
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));  // Finds the crash.

  EXPECT_THAT(output,
              ContainsRegex(R"(SUMMARY: libFuzzer: fuzz target exited)"))
      << output;  // Print unescaped output for easier debugging.

  std::string solution;
  EXPECT_TRUE(base::ReadFileToString(solution_path, &solution));
  EXPECT_EQ(solution, "fish");
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
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  base::FilePath solution_path = dir.GetPath().AppendASCII("solution");

  base::CommandLine cmd(FuzzerPath("string_compare_proto_fuzzer"));
  cmd.AppendArg("-max_total_time=5");
  cmd.AppendArg("-exact_artifact_path=" + solution_path.MaybeAsASCII());

  std::string output;
  EXPECT_FALSE(base::GetAppOutputAndError(cmd, &output));  // Finds the crash.

  EXPECT_THAT(output,
              ContainsRegex(R"(SUMMARY: libFuzzer: fuzz target exited)"))
      << output;  // Print unescaped output for easier debugging.

  std::string solution;
  EXPECT_TRUE(base::ReadFileToString(solution_path, &solution));
  EXPECT_EQ(solution, "\012\004fish");
}

}  // namespace
