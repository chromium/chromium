// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/command_line.h"
#include "base/notreached.h"
#include "remoting/host/base/host_exit_codes.h"

namespace {

constexpr char kEvaluateTest[] = "test";
constexpr char kEvaluateCrash[] = "crash";
constexpr char kEvaluateSuccess[] = "success";
constexpr char kEvaluateCapabilitySwitchName[] = "evaluate-type";
const int kInvalidCommandLineExitCode = 3;

// This function is for test purpose only. It writes some random texts to both
// stdout and stderr, and returns a random value 234.
int EvaluateTest() {
  std::cout << "In EvaluateTest(): Line 1\n"
               "In EvaluateTest(): Line 2";
  std::cerr << "In EvaluateTest(): Error Line 1\n"
               "In EvaluateTest(): Error Line 2";
  return 234;
}

// This function is for test purpose only. It triggers an assertion failure.
int EvaluateCrash() {
  NOTREACHED();
}

// This function is for test purpose only. It writes "Success" to stdout, and
// returns 0.
int EvaluateSuccess() {
  std::cout << "Success" << std::endl;
  return 0;
}

int EvaluateCapabilityLocally(const std::string& type) {
  if (type == kEvaluateTest) {
    return EvaluateTest();
  }
  if (type == kEvaluateCrash) {
    return EvaluateCrash();
  }
  if (type == kEvaluateSuccess) {
    return EvaluateSuccess();
  }

  return kInvalidCommandLineExitCode;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kEvaluateCapabilitySwitchName)) {
    return EvaluateCapabilityLocally(
        command_line->GetSwitchValueASCII(kEvaluateCapabilitySwitchName));
  }

  return kInvalidCommandLineExitCode;
}
