// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_PERMISSIONS_MANAGER_WAITER_H_
#define EXTENSIONS_TEST_PERMISSIONS_MANAGER_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/browser/permissions_manager.h"

namespace extensions {

class PermissionsManagerWaiter : public PermissionsManager::Observer {
 public:
  explicit PermissionsManagerWaiter(PermissionsManager* manager);
  PermissionsManagerWaiter(const PermissionsManagerWaiter&) = delete;
  const PermissionsManagerWaiter& operator=(const PermissionsManagerWaiter&) =
      delete;
  ~PermissionsManagerWaiter();

  // Waits until permissions change.
  void WaitForPermissionsChange();

 private:
  // PermissionsManager::Observer:
  void UserPermissionsSettingsChanged(
      const PermissionsManager::UserPermissionsSettings& settings) override;

  base::RunLoop run_loop_;
  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      manager_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_PERMISSIONS_MANAGER_WAITER_H_
