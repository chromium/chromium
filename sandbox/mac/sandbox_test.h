// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SANDBOX_TEST_H_
#define SANDBOX_MAC_SANDBOX_TEST_H_

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"

namespace sandbox {

// Base class for multiprocess sandbox tests. This exists to override some
// command line preparation behavior for spawned child processes.
class SandboxTest : public base::MultiProcessTest {
 public:
  using CommandLineModifier =
      std::optional<base::RepeatingCallback<void(base::CommandLine&)>>;

  SandboxTest();
  ~SandboxTest() override;

  // Launches a new test child process to run `procname`. If
  // `command_line_modifier` is not null, it will be run to modify the child
  // command line immediately before launch. Returns a handle to the launched
  // process.
  base::Process SpawnChildWithOptions(
      const std::string& procname,
      const base::LaunchOptions& options,
      CommandLineModifier command_line_modifier = std::nullopt);

  // Same as SpawnChildWithOptions, but uses a default LaunchOptions value.
  base::Process SpawnChild(
      const std::string& procname,
      CommandLineModifier command_line_modifier = std::nullopt);
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SANDBOX_TEST_H_
