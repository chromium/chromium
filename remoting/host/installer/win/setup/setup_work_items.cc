// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/setup_work_items.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "remoting/host/installer/win/setup/file_work_items.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/registry_work_items.h"
#include "remoting/host/installer/win/setup/service_work_items.h"

namespace remoting::installer {

std::unique_ptr<WorkItemList> CreateInstallWorkItemList(
    const base::FilePath& source_dir,
    const base::FilePath& install_dir,
    const base::FilePath& host_config_path) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  list->AddCreateDirWorkItem(install_dir);
  list->AddWorkItem(new StopServiceWorkItem());
  list->AddWorkItem(new DeployFilesWorkItem(source_dir, install_dir));
  list->AddWorkItem(new HostRegistryWorkItem(install_dir));
  base::FilePath host_binary = install_dir.Append(kHostBinaryName);
  list->AddWorkItem(
      new InstallOrUpdateServiceWorkItem(host_binary, host_config_path));
  list->AddWorkItem(new StartServiceWorkItem());

  return list;
}

std::unique_ptr<WorkItemList> CreateUpdatePendingWorkItemList(
    const base::FilePath& source_dir,
    const base::FilePath& install_dir,
    std::wstring event_name_for_testing) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  list->AddCreateDirWorkItem(install_dir);
  list->AddWorkItem(new DeployFilesWorkItem(source_dir, install_dir));
  list->AddWorkItem(new HostRegistryWorkItem(install_dir, /*is_update=*/true));
  list->AddWorkItem(
      new SignalUpdatePendingWorkItem(std::move(event_name_for_testing)));

  return list;
}

std::unique_ptr<WorkItemList> CreateUninstallWorkItemList(
    const base::FilePath& install_dir) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  list->set_rollback_enabled(false);

  list->AddWorkItem(new StopServiceWorkItem());
  list->AddWorkItem(new UninstallServiceWorkItem());
  list->AddWorkItem(new UnregisterRegistryWorkItem());
  list->AddWorkItem(new DeleteFilesWorkItem(install_dir));

  return list;
}

}  // namespace remoting::installer
