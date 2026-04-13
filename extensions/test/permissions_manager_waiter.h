// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_PERMISSIONS_MANAGER_WAITER_H_
#define EXTENSIONS_TEST_PERMISSIONS_MANAGER_WAITER_H_

#include <optional>
#include <set>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class PermissionsManagerWaiter : public PermissionsManager::Observer {
 public:
  using ExtensionPermissionUpdateCallback =
      base::OnceCallback<void(const Extension& extension,
                              const PermissionSet& permissions,
                              PermissionsManager::UpdateReason reason)>;
  explicit PermissionsManagerWaiter(PermissionsManager* manager);
  PermissionsManagerWaiter(const PermissionsManagerWaiter&) = delete;
  const PermissionsManagerWaiter& operator=(const PermissionsManagerWaiter&) =
      delete;
  ~PermissionsManagerWaiter() override;

  // Waits until permissions change.
  void WaitForUserPermissionsSettingsChange();
  void WaitForExtensionPermissionsUpdate();
  // Waits until extension permissions change and then calls `callback`.
  void WaitForExtensionPermissionsUpdate(
      ExtensionPermissionUpdateCallback callback);

  // Waits until `extension_id` is granted active tab permission.
  void WaitForActiveTabPermissionGranted(const ExtensionId& extension_id);

 private:
  // PermissionsManager::Observer:
  void OnUserPermissionsSettingsChanged(
      const PermissionsManager::UserPermissionsSettings& settings) override;
  void OnExtensionPermissionsUpdated(
      const Extension& extension,
      const PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override;
  void OnActiveTabPermissionGranted(const Extension& extension) override;

  base::RunLoop user_permissions_settings_changed_run_loop_;
  base::RunLoop extension_permissions_update_run_loop_;
  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      manager_observation_{this};
  ExtensionPermissionUpdateCallback extension_permissions_update_callback_;

  std::optional<ExtensionId> waiting_for_active_tab_extension_id_;
  std::set<ExtensionId> granted_active_tab_extensions_;
  base::RunLoop active_tab_permission_granted_run_loop_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_PERMISSIONS_MANAGER_WAITER_H_
