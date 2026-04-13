// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/permissions_manager_waiter.h"

#include "base/check.h"

namespace extensions {

PermissionsManagerWaiter::PermissionsManagerWaiter(
    PermissionsManager* manager) {
  manager_observation_.Observe(manager);
}

PermissionsManagerWaiter::~PermissionsManagerWaiter() = default;

void PermissionsManagerWaiter::WaitForUserPermissionsSettingsChange() {
  user_permissions_settings_changed_run_loop_.Run();
}

void PermissionsManagerWaiter::WaitForExtensionPermissionsUpdate() {
  extension_permissions_update_run_loop_.Run();
}

void PermissionsManagerWaiter::WaitForExtensionPermissionsUpdate(
    base::OnceCallback<void(const Extension& extension,
                            const PermissionSet& permissions,
                            PermissionsManager::UpdateReason reason)>
        callback) {
  extension_permissions_update_callback_ = std::move(callback);
  WaitForExtensionPermissionsUpdate();
}

void PermissionsManagerWaiter::WaitForActiveTabPermissionGranted(
    const ExtensionId& extension_id) {
  if (granted_active_tab_extensions_.contains(extension_id)) {
    return;
  }
  CHECK(!waiting_for_active_tab_extension_id_.has_value());
  waiting_for_active_tab_extension_id_ = extension_id;
  active_tab_permission_granted_run_loop_.Run();
  waiting_for_active_tab_extension_id_.reset();
}

void PermissionsManagerWaiter::OnUserPermissionsSettingsChanged(
    const PermissionsManager::UserPermissionsSettings& settings) {
  user_permissions_settings_changed_run_loop_.Quit();
}

void PermissionsManagerWaiter::OnExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsManager::UpdateReason reason) {
  if (extension_permissions_update_callback_) {
    std::move(extension_permissions_update_callback_)
        .Run(extension, permissions, reason);
  }
  extension_permissions_update_run_loop_.Quit();
}

void PermissionsManagerWaiter::OnActiveTabPermissionGranted(
    const Extension& extension) {
  granted_active_tab_extensions_.insert(extension.id());
  if (waiting_for_active_tab_extension_id_ == extension.id()) {
    active_tab_permission_granted_run_loop_.Quit();
  }
}

}  // namespace extensions
