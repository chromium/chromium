// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/tests/fuzz_target.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "testing/libfuzzer/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace fuzzing {
namespace {

// Signal-handling options that sanitizers leave off by default. ClusterFuzz
// sets them when it runs fuzzers, so these tests set them too to match.
// Without them, a CHECK failure (SIGABRT on POSIX; a breakpoint exception on
// Windows, which ASan only reports with handle_sigill) may not reach a handler
// that makes the engine save the crashing input.
constexpr std::string_view kSignalHandlingSanitizerOptions =
    "handle_abort=1 handle_sigill=1";

constexpr base::cstring_view kSanitizerOptionsVariables[] = {
    "ASAN_OPTIONS",
    "MSAN_OPTIONS",
    "UBSAN_OPTIONS",
};

base::FilePath BinaryPath(std::string_view file_name) {
  base::FilePath out_dir;
  base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &out_dir);

  return out_dir.AppendASCII(file_name);
}

// Returns options that launch a fuzzer with `kSignalHandlingSanitizerOptions`
// appended to the sanitizer options inherited from this process.
base::LaunchOptions FuzzLaunchOptions() {
  base::Environment env;
  base::LaunchOptions options;
  for (base::cstring_view variable : kSanitizerOptionsVariables) {
    std::string value = base::StrCat({env.GetVar(variable).value_or(""), " ",
                                      kSignalHandlingSanitizerOptions});
#if BUILDFLAG(IS_WIN)
    options.environment[base::UTF8ToWide(variable)] = base::UTF8ToWide(value);
#else
    options.environment[std::string(variable)] = std::move(value);
#endif
  }
  return options;
}

}  // namespace

FuzzTarget::FuzzTarget(std::string_view fuzzer_name)
    : fuzz_target_path_(BinaryPath(fuzzer_name)) {}

// static
std::optional<FuzzTarget> FuzzTarget::Make(std::string_view fuzzer_name) {
  FuzzTarget target(fuzzer_name);
  if (!target.temp_dir_.CreateUniqueTempDir()) {
    return std::nullopt;
  }

  return target;
}

base::CommandLine FuzzTarget::LibfuzzerCommandLine() const {
  base::CommandLine cmd(fuzz_target_path_);
  cmd.AppendArg("-runs=10");
  cmd.AppendArg(base::StrCat({
      "-artifact_prefix=",
      temp_dir_.GetPath().AppendASCII("crash-").MaybeAsASCII(),
  }));
  cmd.AppendArg("-rss_limit_mb=0");
  return cmd;
}

base::CommandLine FuzzTarget::CentipedeCommandLine() const {
  base::CommandLine cmd(BinaryPath("centipede"));
  cmd.AppendArg("--j=1");
  cmd.AppendArg(base::StrCat({
      "--binary=",
      fuzz_target_path_.MaybeAsASCII(),
  }));
  cmd.AppendArg("--num_runs=10");
  cmd.AppendArg(base::StrCat({
      "--workdir=",
      temp_dir_.GetPath().MaybeAsASCII(),
  }));
  return cmd;
}

base::CommandLine FuzzTarget::FuzzCommandLine() const {
#if BUILDFLAG(USE_CENTIPEDE)
  return CentipedeCommandLine();
#else
  return LibfuzzerCommandLine();
#endif
}

bool FuzzTarget::Fuzz() {
  base::Process process =
      base::LaunchProcess(FuzzCommandLine(), FuzzLaunchOptions());
  CHECK(process.IsValid());
  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    return false;
  }
  return exit_code == 0;
}

base::FilePath FuzzTarget::CrashingInputsDir() const {
#if BUILDFLAG(USE_CENTIPEDE)
  return CentipedeCrashingInputsDir();
#else
  return LibfuzzerCrashingInputsDir();
#endif
}

base::FilePath FuzzTarget::LibfuzzerCrashingInputsDir() const {
  return temp_dir_.GetPath();
}

base::FilePath FuzzTarget::CentipedeCrashingInputsDir() const {
  return temp_dir_.GetPath().AppendASCII("crashes.000000");
}

std::vector<std::string> FuzzTarget::GetCrashingInputs() const {
  constexpr bool kNotRecursive = false;
  base::FileEnumerator e(CrashingInputsDir(), kNotRecursive,
                         base::FileEnumerator::FILES);

  std::vector<std::string> inputs;
  for (base::FilePath path = e.Next(); !path.empty(); path = e.Next()) {
    std::string contents;
    if (base::ReadFileToString(path, &contents)) {
      inputs.push_back(std::move(contents));
    } else {
      // Add the error to the return value. Typically tests will check the
      // values for equality, and this will surface the error.
      inputs.push_back(base::StrCat({
          "error: failed to read ",
          path.MaybeAsASCII(),
      }));
    }
  }

  return inputs;
}

}  // namespace fuzzing
