// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/registry_work_items.h"

#include <windows.h>

#include <string>

#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/registry_util.h"

namespace remoting::installer {

HostRegistryWorkItem::HostRegistryWorkItem(const base::FilePath& install_dir,
                                           bool is_update)
    : install_dir_(install_dir), is_update_(is_update) {}

HostRegistryWorkItem::~HostRegistryWorkItem() = default;

bool HostRegistryWorkItem::DoImpl() {
  if (!is_update_) {
    base::win::RegKey key;
    std::wstring chromoting_key =
        base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
    if (key.Open(HKEY_LOCAL_MACHINE, chromoting_key.c_str(),
                 KEY_READ | KEY_WOW64_32KEY) == ERROR_SUCCESS ||
        key.Open(HKEY_LOCAL_MACHINE, chromoting_key.c_str(),
                 KEY_READ | KEY_WOW64_64KEY) == ERROR_SUCCESS) {
      is_update_ = true;
    }
  }
  return RegisterHostRegistryEntries(install_dir_);
}

void HostRegistryWorkItem::RollbackImpl() {
  if (is_update_) {
    // Preserve existing host registration and configuration during an update
    // failure so we do not wipe out the user's host pairing.
    // TODO(crbug.com/551615326): Refactor `RegisterHostRegistryEntries()` to
    // populate a `WorkItemList` using `CreateRegKeyWorkItem` and
    // `SetRegValueWorkItem` so partial writes during an update are restored to
    // their previous values on rollback.
    return;
  }
  UnregisterHostRegistryEntries();
}

UnregisterRegistryWorkItem::UnregisterRegistryWorkItem() = default;

UnregisterRegistryWorkItem::~UnregisterRegistryWorkItem() = default;

bool UnregisterRegistryWorkItem::DoImpl() {
  UnregisterHostRegistryEntries();
  return true;
}

void UnregisterRegistryWorkItem::RollbackImpl() {
  // Uninstallation is best-effort and runs with `set_rollback_enabled(false)`.
  // If an uninstallation step fails, we leave already-removed entries
  // unregistered rather than attempting to re-register a partially removed
  // host.
  // TODO(crbug.com/551615326): Evaluate replacing `UnregisterRegistryWorkItem`
  // with `DeleteRegKeyWorkItem` / `DeleteRegValueWorkItem` items once
  // `registry_util` is refactored to use `WorkItemList`.
}

}  // namespace remoting::installer
