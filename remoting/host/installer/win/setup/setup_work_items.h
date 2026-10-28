// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_SETUP_WORK_ITEMS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_SETUP_WORK_ITEMS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item_list.h"

namespace remoting::installer {

// Builds the complete WorkItemList for installing or updating Chrome Remote
// Desktop on a machine where the host is idle or not yet installed, or when an
// update modifies the Windows service configuration.
// When `work_item_list->Do()` executes:
// - Creates `install_dir`.
// - Stops host service if running.
// - Deploys payload files with staged rename.
// - Registers COM & Native Messaging registry entries.
// - Configures host Windows service.
// - Starts host Windows service.
// On failure, `work_item_list->Rollback()` rolls back all completed actions
// in reverse order.
std::unique_ptr<WorkItemList> CreateInstallWorkItemList(
    const base::FilePath& source_dir,
    const base::FilePath& install_dir,
    const base::FilePath& host_config_path);

// Builds the WorkItemList for staging an update when a host desktop session is
// currently active.
// When `work_item_list->Do()` executes:
// - Creates `install_dir`.
// - Deploys payload files with staged rename (moving locked files to .old).
// - Registers COM & Native Messaging registry entries.
// - Signals the global CrdHostUpdatePending event.
// The host service is NOT stopped, allowing the active session to continue.
std::unique_ptr<WorkItemList> CreateUpdatePendingWorkItemList(
    const base::FilePath& source_dir,
    const base::FilePath& install_dir,
    std::wstring event_name_for_testing = std::wstring());

// Builds the complete WorkItemList for uninstalling Chrome Remote Desktop.
// When `work_item_list->Do()` executes:
// - Stops host service.
// - Uninstalls host service.
// - Unregisters registry entries.
// - Deletes installed payload files and install dir.
std::unique_ptr<WorkItemList> CreateUninstallWorkItemList(
    const base::FilePath& install_dir);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_SETUP_WORK_ITEMS_H_
