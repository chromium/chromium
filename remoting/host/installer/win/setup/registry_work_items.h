// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_WORK_ITEMS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_WORK_ITEMS_H_

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"

namespace remoting::installer {

// A WorkItem that registers Chrome Remote Desktop registry entries
// (Native Messaging hosts, COM AppId/CLSID/Interface/TypeLib, EventLog,
// Omaha, URL forwarder, and paired-client keys). On rollback, all registered
// entries are unregistered, unless `is_update` is true (or automatically
// promoted to true during `Do()` when an existing installation is detected in
// the registry) to preserve existing host settings and paired clients.
// TODO(crbug.com/551615326): Refactor `RegisterHostRegistryEntries()` into a
// `WorkItemList` of `CreateRegKeyWorkItem` and `SetRegValueWorkItem` instances
// so partial failures and in-place updates restore exact pre-existing registry
// values on rollback.
class HostRegistryWorkItem : public WorkItem {
 public:
  explicit HostRegistryWorkItem(const base::FilePath& install_dir,
                                bool is_update = false);
  HostRegistryWorkItem(const HostRegistryWorkItem&) = delete;
  HostRegistryWorkItem& operator=(const HostRegistryWorkItem&) = delete;
  ~HostRegistryWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  base::FilePath install_dir_;
  bool is_update_ = false;
};

// A WorkItem that unregisters Chrome Remote Desktop registry entries during
// uninstallation. Uninstallation is executed on a best-effort basis with
// rollback disabled, so `RollbackImpl()` is intentionally a no-op.
class UnregisterRegistryWorkItem : public WorkItem {
 public:
  UnregisterRegistryWorkItem();
  UnregisterRegistryWorkItem(const UnregisterRegistryWorkItem&) = delete;
  UnregisterRegistryWorkItem& operator=(const UnregisterRegistryWorkItem&) =
      delete;
  ~UnregisterRegistryWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;
};

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_WORK_ITEMS_H_
