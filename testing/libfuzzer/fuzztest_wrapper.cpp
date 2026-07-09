// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A wrapper which knows to execute a given fuzzer within a fuzztest
// executable that contains multiple fuzzers.
// The fuzzer binary is assumed to be in the same directory as this binary.

#include <cerrno>
#include <iostream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/libfuzzer/buildflags.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>

#include "base/posix/safe_strerror.h"
#endif

extern const char* kFuzzerBinary;
extern const char* kFuzzerArgs;

namespace {

constexpr int kErrorExitCode = -1;  // equal to 255

#if BUILDFLAG(USE_CENTIPEDE)

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

#endif  // BUILDFLAG(USE_CENTIPEDE)

}  // namespace

int main(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  base::FilePath fuzzer_path;
  if (!base::PathService::Get(base::DIR_EXE, &fuzzer_path)) {
    std::cerr << "[FuzzTest Wrapper] ERROR: Failed to obtain base::DIR_EXE via "
                 "PathService.\n";
    return kErrorExitCode;
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
  // Pre-flight diagnostic checks before launching the underlying test binary.
  if (!base::PathExists(fuzzer_path)) {
    std::cerr
        << "[FuzzTest Wrapper] ERROR: Target fuzzer binary not found at path: "
        << fuzzer_path.value() << "\n"
        << "[FuzzTest Wrapper] Please ensure that '" << kFuzzerBinary
        << "' is listed in this wrapper's .runtime_deps and was unpacked "
           "correctly by the bot.\n";
    return kErrorExitCode;
  }

#if BUILDFLAG(IS_POSIX)
  if (access(fuzzer_path.value().c_str(), X_OK) != 0) {
    int access_err = errno;
    std::cerr << "[FuzzTest Wrapper] ERROR: Target fuzzer binary exists at "
              << fuzzer_path.value() << " but is not executable (errno "
              << access_err << ": " << base::safe_strerror(access_err)
              << ").\n";
    return kErrorExitCode;
  }
#endif

  std::cerr << "FuzzTest wrapper launching:" << cmdline.GetCommandLineString()
            << "\n";
  base::Process p = base::LaunchProcess(cmdline, launch_options);
  if (!p.IsValid()) {
    std::cerr
        << "[FuzzTest Wrapper] ERROR: base::LaunchProcess failed to launch: "
        << fuzzer_path.value() << "\n"
        << "[FuzzTest Wrapper] Check system logs or execvp/CreateProcess "
           "errors above.\n";
    return kErrorExitCode;
  }
  int exit_code;
  if (!p.WaitForExit(&exit_code)) {
    std::cerr
        << "[FuzzTest Wrapper] ERROR: WaitForExit failed on child process.\n";
    return kErrorExitCode;
  }
  if (exit_code != 0) {
    std::cerr << "[FuzzTest Wrapper] NOTICE: Underlying fuzzer binary exited "
                 "with non-zero code "
              << exit_code << "\n";
  }
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
