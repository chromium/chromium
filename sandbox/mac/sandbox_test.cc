// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_test.h"

#include "base/base_switches.h"
#include "base/process/launch.h"
#include "mojo/core/test/test_switches.h"

namespace sandbox {

SandboxTest::SandboxTest() = default;

SandboxTest::~SandboxTest() = default;

base::Process SandboxTest::SpawnChild(
    const std::string& procname,
    CommandLineModifier command_line_modifier) {
  return SpawnChildWithOptions(procname, base::LaunchOptions{},
                               std::move(command_line_modifier));
}

base::Process SandboxTest::SpawnChildWithOptions(
    const std::string& procname,
    const base::LaunchOptions& options,
    CommandLineModifier command_line_modifier) {
  base::CommandLine command_line(MakeCmdLine(procname));

  // NOTE: Mojo initialization fails inside the test sandbox configuration
  // due to an internal Abseil dependency on sysctl(). We don't use Mojo in
  // these test child processes, so suppress its initialization there.
  command_line.AppendSwitch(test_switches::kNoMojo);
  if (command_line_modifier) {
    command_line_modifier->Run(command_line);
  }
  return base::SpawnMultiProcessTestChild(procname, command_line, options);
}

}  // namespace sandbox
