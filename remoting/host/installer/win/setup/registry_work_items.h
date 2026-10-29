// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_WORK_ITEMS_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_WORK_ITEMS_H_

// Chrome Remote Desktop product-specific Windows registry `WorkItem`s and
// registration schemas for the Windows host installer.
//
// What belongs here:
// - `HostRegistryWorkItem` and `UnregisterRegistryWorkItem`, along with all
//   CRD-specific registry subsystem registrations (Native Messaging hosts,
//   Windows EventLog source, Remote Security Key, URL forwarder, COM
//   AppId/CLSID/Interface/TypeLib, pairing/logging keys, and Omaha Clients).
//
// What does NOT belong here:
// - Generic, product-agnostic registry helpers or primitive `WorkItem`
//   building blocks (e.g., `SetRegBinaryValueWorkItem`,
//   `CreateProtectedRegKeyWorkItem`,
//   `AddCreateKeyAndSetDefaultValueWorkItems`). Put those in
//   `registry_util.{h,cc}`.

#include <memory>

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace remoting::installer {

// A WorkItem that registers Chrome Remote Desktop registry entries
// (Native Messaging hosts, COM AppId/CLSID/Interface/TypeLib, EventLog,
// Omaha, URL forwarder, and paired-client keys) via an internal `WorkItemList`
// of `CreateRegKeyWorkItem`, `SetRegValueWorkItem`,
// `SetRegBinaryValueWorkItem`, and `CreateProtectedRegKeyWorkItem` instances.
// On rollback:
// - Newly created keys and values (on a fresh install or when new entries are
//   added) are deleted.
// - Pre-existing keys (including `paired-clients\clients` and
//   `paired-clients\secrets`) are preserved and any overwritten values are
//   restored to their exact prior values.
class HostRegistryWorkItem : public WorkItem {
 public:
  explicit HostRegistryWorkItem(const base::FilePath& install_dir);
  HostRegistryWorkItem(const HostRegistryWorkItem&) = delete;
  HostRegistryWorkItem& operator=(const HostRegistryWorkItem&) = delete;
  ~HostRegistryWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  base::FilePath install_dir_;
  std::unique_ptr<WorkItemList> work_item_list_;
};

// A WorkItem that unregisters Chrome Remote Desktop registry entries during
// uninstallation via an internal `WorkItemList` of `DeleteRegKeyWorkItem` and
// `DeleteRegValueWorkItem` instances, preserving shared parent keys (such as
// `SOFTWARE\Google\SecurityKey` and `SOFTWARE\RegisteredApplications`) when
// other values or subkeys are present. Uninstallation is executed on a
// best-effort basis with rollback disabled, so `RollbackImpl()` is
// intentionally a no-op.
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

  std::unique_ptr<WorkItemList> work_item_list_;
};

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_WORK_ITEMS_H_
