// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/setup_runner.h"

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/installer/util/work_item_list.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/service_util.h"
#include "remoting/host/installer/win/setup/setup_work_items.h"

namespace remoting::installer {

SetupConfig::SetupConfig() = default;
SetupConfig::SetupConfig(const SetupConfig&) = default;
SetupConfig& SetupConfig::operator=(const SetupConfig&) = default;
SetupConfig::~SetupConfig() = default;

SetupExitCode RunSetup(const base::CommandLine& command_line) {
  return RunSetupWithConfig(command_line, SetupConfig());
}

SetupExitCode RunSetupWithConfig(const base::CommandLine& command_line,
                                 const SetupConfig& config) {
  bool has_install = command_line.HasSwitch(kInstallSwitch);
  bool has_uninstall = command_line.HasSwitch(kUninstallSwitch);

  if (has_install && has_uninstall) {
    LOG(ERROR) << "Cannot combine --install with --uninstall.";
    return SetupExitCode::kInvalidArguments;
  }

  base::FilePath install_dir =
      config.install_dir.empty() ? GetDefaultInstallDir() : config.install_dir;
  if (install_dir.empty()) {
    LOG(ERROR) << "Failed to resolve installation directory.";
    return SetupExitCode::kPathResolutionFailed;
  }

  if (has_uninstall) {
    std::unique_ptr<WorkItemList> list =
        CreateUninstallWorkItemList(install_dir);
    if (!list->Do()) {
      LOG(ERROR) << "Uninstallation pipeline failed.";
      return SetupExitCode::kUninstallFailed;
    }
    return SetupExitCode::kSuccess;
  }

  base::FilePath source_dir = config.source_dir;
  if (source_dir.empty() &&
      !base::PathService::Get(base::DIR_EXE, &source_dir)) {
    LOG(ERROR) << "Failed to resolve source payload directory.";
    return SetupExitCode::kPathResolutionFailed;
  }

  base::FilePath host_config_path = config.host_config_path.empty()
                                        ? GetDefaultHostConfigPath()
                                        : config.host_config_path;
  if (host_config_path.empty()) {
    LOG(ERROR) << "Failed to resolve host configuration file path.";
    return SetupExitCode::kPathResolutionFailed;
  }

  std::unique_ptr<WorkItemList> list;
  if (IsHostSessionActive(config.session_active_event_name.c_str())) {
    list = CreateUpdatePendingWorkItemList(source_dir, install_dir,
                                           host_config_path,
                                           config.update_pending_event_name);
  } else {
    list = CreateInstallWorkItemList(source_dir, install_dir, host_config_path);
  }

  if (!list->Do()) {
    LOG(ERROR) << "Installation pipeline failed; rolling back changes.";
    list->Rollback();
    return SetupExitCode::kInstallFailed;
  }

  return SetupExitCode::kSuccess;
}

}  // namespace remoting::installer
