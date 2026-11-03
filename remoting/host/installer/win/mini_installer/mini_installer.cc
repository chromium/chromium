// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/mini_installer/mini_installer.h"

#include <windows.h>

#include <shlobj.h>

#include <functional>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/lzma_util.h"

namespace remoting::installer {

namespace {

bool CreateExtractionTempDir(const base::FilePath& temp_parent_dir,
                             base::ScopedTempDir& temp_dir) {
  if (!temp_parent_dir.empty()) {
    return temp_dir.CreateUniqueTempDirUnderPath(temp_parent_dir);
  }

  // `base::ScopedTempDir::CreateUniqueTempDir()` only selects `DIR_SYSTEM_TEMP`
  // for `TokenElevationTypeDefault` (e.g. `SYSTEM`), not for UAC split-token
  // admin processes (`TokenElevationTypeFull`). Explicitly resolve
  // `DIR_SYSTEM_TEMP` when running as an admin (or `DIR_TEMP` when unelevated)
  // via `PathService` so unpacked binaries are never written to user-writable
  // `%TEMP%` when elevated. Note that `DIR_SYSTEM_TEMP` falls back to
  // `DIR_PROGRAM_FILES` if `%windir%\SystemTemp` is unavailable.
  base::FilePath parent_dir;
  const int path_key =
      ::IsUserAnAdmin() ? int{base::DIR_SYSTEM_TEMP} : int{base::DIR_TEMP};
  return base::PathService::Get(path_key, &parent_dir) &&
         temp_dir.CreateUniqueTempDirUnderPath(parent_dir);
}

void CleanupExtractionTempDir(base::ScopedTempDir& temp_dir) {
  // Immediately after the child setup process exits, Windows or AV scanners may
  // briefly retain open handles on the unpacked executable or DLLs. Retry
  // deletion briefly before falling back to reboot cleanup.
  constexpr int kMaxDeleteAttempts = 10;
  constexpr base::TimeDelta kDeleteRetryDelay = base::Milliseconds(100);
  for (int attempt = 0; attempt < kMaxDeleteAttempts; ++attempt) {
    if (temp_dir.Delete()) {
      return;
    }
    if (attempt + 1 < kMaxDeleteAttempts) {
      base::PlatformThread::Sleep(kDeleteRetryDelay);
    }
  }

  base::FilePath remaining_path = temp_dir.Take();
  LOG(WARNING) << "Failed to delete temporary directory "
               << remaining_path.value()
               << "; scheduling for deletion at reboot.";
  std::ignore = ScheduleDirectoryForDeletion(remaining_path);
}

int LaunchAndWaitForSetupProcess(const base::CommandLine& setup_command_line) {
  base::LaunchOptions launch_options;
  launch_options.current_directory = setup_command_line.GetProgram().DirName();
  base::Process process =
      base::LaunchProcess(setup_command_line, launch_options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch setup process: "
                << setup_command_line.GetProgram().value();
    return static_cast<int>(MiniInstallerExitCode::kSetupLaunchFailed);
  }

  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    PLOG(ERROR) << "Failed waiting for setup process to exit.";
    return static_cast<int>(MiniInstallerExitCode::kSetupWaitFailed);
  }
  return exit_code;
}

}  // namespace

MiniInstallerConfig::MiniInstallerConfig() = default;
MiniInstallerConfig::MiniInstallerConfig(MiniInstallerConfig&&) = default;
MiniInstallerConfig& MiniInstallerConfig::operator=(MiniInstallerConfig&&) =
    default;
MiniInstallerConfig::~MiniInstallerConfig() = default;

std::optional<base::span<const uint8_t>> LoadResourceSpan(
    HMODULE module,
    const wchar_t* resource_name,
    const wchar_t* resource_type) {
  if (!module || !resource_name || !resource_type) {
    return std::nullopt;
  }

  HRSRC resource_info = FindResourceW(module, resource_name, resource_type);
  if (!resource_info) {
    return std::nullopt;
  }

  DWORD resource_size = SizeofResource(module, resource_info);
  if (resource_size == 0) {
    return std::nullopt;
  }

  HGLOBAL resource_handle = LoadResource(module, resource_info);
  if (!resource_handle) {
    return std::nullopt;
  }

  const void* resource_data = LockResource(resource_handle);
  if (!resource_data) {
    return std::nullopt;
  }

  // SAFETY: `LockResource` returns a pointer to at least `SizeofResource`
  // valid bytes in the loaded module's resource section.
  return UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(resource_data), resource_size));
}

int RunMiniInstallerFromArchiveBuffer(base::span<const uint8_t> archive_buffer,
                                      const base::CommandLine& command_line,
                                      MiniInstallerConfig config) {
  if (config.setup_binary_name.empty()) {
    return static_cast<int>(MiniInstallerExitCode::kSetupBinaryNotFound);
  }

  base::ScopedTempDir temp_dir;
  if (!CreateExtractionTempDir(config.temp_parent_dir, temp_dir)) {
    LOG(ERROR) << "Failed to create temporary extraction directory.";
    return static_cast<int>(MiniInstallerExitCode::kTempDirCreationFailed);
  }
  base::ScopedClosureRunner cleanup_temp_dir(
      base::BindOnce(&CleanupExtractionTempDir, std::ref(temp_dir)));

  UnPackStatus unpack_status = UnPackArchiveBuffer(
      archive_buffer, temp_dir.GetPath(), /*output_file=*/nullptr);
  if (unpack_status != UNPACK_NO_ERROR) {
    LOG(ERROR) << "Failed to unpack installer archive: " << unpack_status;
    return static_cast<int>(MiniInstallerExitCode::kUnpackArchiveFailed);
  }

  base::FilePath setup_path =
      temp_dir.GetPath().Append(config.setup_binary_name);
  if (!base::PathExists(setup_path)) {
    LOG(ERROR) << "Unpacked archive is missing setup binary: "
               << setup_path.value();
    return static_cast<int>(MiniInstallerExitCode::kSetupBinaryNotFound);
  }

  base::CommandLine setup_command_line(setup_path);
  setup_command_line.AppendArguments(command_line, /*include_program=*/false);

  return config.setup_runner
             ? std::move(config.setup_runner).Run(setup_command_line)
             : LaunchAndWaitForSetupProcess(setup_command_line);
}

int RunMiniInstaller(HMODULE module, const base::CommandLine& command_line) {
  std::optional<base::span<const uint8_t>> archive_span =
      LoadResourceSpan(module, kArchiveResourceName, kArchiveResourceType);
  if (!archive_span.has_value()) {
    LOG(ERROR) << "Embedded installer archive resource not found.";
    return static_cast<int>(MiniInstallerExitCode::kResourceNotFound);
  }

  return RunMiniInstallerFromArchiveBuffer(*archive_span, command_line);
}

}  // namespace remoting::installer
