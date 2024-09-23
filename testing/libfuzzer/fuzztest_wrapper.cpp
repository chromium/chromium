// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A wrapper which knows to execute a given fuzzer within a fuzztest
// executable that contains multiple fuzzers.
// The fuzzer binary is assumed to be in the same directory as this binary.

#include <iostream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/libfuzzer/fuzztest_wrapper_buildflags.h"

extern const char* kFuzzerBinary;
extern const char* kFuzzerArgs;

#if BUILDFLAG(USE_CENTIPEDE)

namespace {
void HandleReplayMode(auto& args) {
  // We're handling a centipede based fuzzer. If the last argument is a
  // filepath, we're trying to replay a testcase, since it doesn't make sense
  // to get a filepath when running with the centipede binary.
  if (args.size() <= 1) {
    return;
  }
  base::FilePath test_case(args.back());
  if (!base::PathExists(test_case)) {
    return;
  }

  auto env = base::Environment::Create();
#if BUILDFLAG(IS_WIN)
  auto env_value = base::WideToUTF8(args.back());
#else
  auto env_value = args.back();
#endif
  env->SetVar("FUZZTEST_REPLAY", env_value);
  env->UnSetVar("CENTIPEDE_RUNNER_FLAGS");
  std::cerr << "FuzzTest wrapper setting env var: FUZZTEST_REPLAY="
            << args.back() << '\n';

  // We must not add the testcase to the command line, as this will not be
  // parsed correctly by centipede.
  args.pop_back();
}
}  // namespace

#endif  // BUILDFLAG(USE_CENTIPEDE)

int main(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  base::FilePath fuzzer_path;
  if (!base::PathService::Get(base::DIR_EXE, &fuzzer_path)) {
    return -1;
  }
  fuzzer_path = fuzzer_path.AppendASCII(kFuzzerBinary);
  base::LaunchOptions launch_options;
  base::CommandLine cmdline(fuzzer_path);
  std::vector<std::string_view> additional_args = base::SplitStringPiece(
      kFuzzerArgs, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (auto arg : additional_args) {
    cmdline.AppendArg(arg);
  }
  auto args = base::CommandLine::ForCurrentProcess()->argv();
#if BUILDFLAG(USE_CENTIPEDE)
  HandleReplayMode(args);
#endif  // BUILDFLAG(USE_CENTIPEDE)

  bool skipped_first = false;
  for (auto arg : args) {
    if (!skipped_first) {
      skipped_first = true;
      continue;
    }
    // We avoid AppendArguments because it parses switches then reorders things.
    cmdline.AppendArgNative(arg);
  }
  std::cerr << "FuzzTest wrapper launching:" << cmdline.GetCommandLineString()
            << "\n";
  base::Process p = base::LaunchProcess(cmdline, launch_options);
  int exit_code;
  p.WaitForExit(&exit_code);
  return exit_code;
}

#if defined(WIN32)
#define ALWAYS_EXPORT __declspec(dllexport)
#else
#define ALWAYS_EXPORT __attribute__((visibility("default")))
#endif

ALWAYS_EXPORT extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,
                                                    size_t size) {
  // No-op. This symbol exists to ensure that this binary is detected as
  // a fuzzer by ClusterFuzz's heuristics. It never actually gets called.
  return -1;
}
