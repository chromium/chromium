// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/tests/fuzz_target.h"

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/libfuzzer/buildflags.h"

namespace fuzzing {
namespace {

base::FilePath BinaryPath(std::string_view file_name) {
  base::FilePath out_dir;
  base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &out_dir);

  return out_dir.AppendASCII(file_name);
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

base::CommandLine FuzzTarget::LibfuzzerCommandLine(
    const FuzzOptions& options) const {
  base::CommandLine cmd(fuzz_target_path_);
  cmd.AppendArg(base::StrCat({
      "-max_total_time=",
      base::NumberToString(options.timeout_secs),
  }));
  cmd.AppendArg(base::StrCat({
      "-artifact_prefix=",
      temp_dir_.GetPath().AppendASCII("crash-").MaybeAsASCII(),
  }));
  return cmd;
}

base::CommandLine FuzzTarget::CentipedeCommandLine(
    const FuzzOptions& options) const {
  base::CommandLine cmd(BinaryPath("centipede"));
  cmd.AppendArg("--j=1");
  cmd.AppendArg(base::StrCat({
      "--binary=",
      fuzz_target_path_.MaybeAsASCII(),
  }));
  cmd.AppendArg(base::StrCat({
      "--stop_after=",
      base::NumberToString(options.timeout_secs),
      "s",
  }));
  cmd.AppendArg(base::StrCat({
      "--workdir=",
      temp_dir_.GetPath().MaybeAsASCII(),
  }));
  return cmd;
}

base::CommandLine FuzzTarget::FuzzCommandLine(
    const FuzzOptions& options) const {
#if BUILDFLAG(USE_CENTIPEDE)
  return CentipedeCommandLine(options);
#else
  return LibfuzzerCommandLine(options);
#endif
}

bool FuzzTarget::Fuzz(const FuzzOptions& options) {
  return base::GetAppOutputAndError(FuzzCommandLine(options), &output_);
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
