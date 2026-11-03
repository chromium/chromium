// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_MINI_INSTALLER_MINI_INSTALLER_H_
#define REMOTING_HOST_INSTALLER_WIN_MINI_INSTALLER_MINI_INSTALLER_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/win/windows_types.h"
#include "remoting/host/installer/win/mini_installer/mini_installer_constants.h"

namespace base {
class CommandLine;
}  // namespace base

namespace remoting::installer {

// Callback invoked after unpacking the archive to execute `remoting_setup.exe`
// with the constructed command line and return its exit code.
using SetupRunnerCallback =
    base::OnceCallback<int(const base::CommandLine& setup_command_line)>;

// Optional overrides for executing `RunMiniInstallerFromArchiveBuffer` in
// tests.
struct MiniInstallerConfig {
  MiniInstallerConfig();
  MiniInstallerConfig(MiniInstallerConfig&&);
  MiniInstallerConfig& operator=(MiniInstallerConfig&&);
  ~MiniInstallerConfig();

  // Parent directory under which the unique extraction directory is created.
  // If empty, `base::DIR_SYSTEM_TEMP` is used when running elevated
  // (`::IsUserAnAdmin()`) or `base::DIR_TEMP` when running unelevated.
  base::FilePath temp_parent_dir;

  // Name of the setup binary inside the unpacked archive.
  std::wstring setup_binary_name = kSetupBinaryName;

  // Optional callback invoked instead of launching the setup process.
  SetupRunnerCallback setup_runner;
};

// Locates a resource with `resource_name` and `resource_type` in `module` and
// returns a byte span viewing the resource's memory, or `std::nullopt` if the
// resource does not exist or is empty.
std::optional<base::span<const uint8_t>> LoadResourceSpan(
    HMODULE module,
    const wchar_t* resource_name,
    const wchar_t* resource_type);

// Unpacks `archive_buffer` into a newly created temporary directory, constructs
// a command line for `<temp_dir>\<config.setup_binary_name>` with all switches
// and positional arguments forwarded from `command_line`, invokes
// `config.setup_runner` (or launches the setup process and waits for completion
// when `config.setup_runner` is null), cleans up the temporary directory, and
// returns the exit code.
int RunMiniInstallerFromArchiveBuffer(base::span<const uint8_t> archive_buffer,
                                      const base::CommandLine& command_line,
                                      MiniInstallerConfig config = {});

// Loads the embedded `REMOTING.7Z` (`B7`) resource from `module`, unpacks it
// to a temporary directory, runs `remoting_setup.exe` with switches and
// arguments forwarded from `command_line`, and returns the resulting exit code.
int RunMiniInstaller(HMODULE module, const base::CommandLine& command_line);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_MINI_INSTALLER_MINI_INSTALLER_H_
