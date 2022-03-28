// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/permissions_manager_waiter.h"

namespace extensions {

PermissionsManagerWaiter::PermissionsManagerWaiter(
    PermissionsManager* manager) {
  manager_observation_.Observe(manager);
}

PermissionsManagerWaiter::~PermissionsManagerWaiter() = default;

void PermissionsManagerWaiter::WaitForPermissionsChange() {
  run_loop_.Run();
}

void PermissionsManagerWaiter::UserPermissionsSettingsChanged(
    const PermissionsManager::UserPermissionsSettings& settings) {
  run_loop_.Quit();
}

}  // namespace extensions
