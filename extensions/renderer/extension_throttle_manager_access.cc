// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_throttle_manager_access.h"

#include "base/synchronization/lock.h"
#include "extensions/renderer/extension_throttle_manager.h"

namespace extensions {

ExtensionThrottleManagerAccess::ExtensionThrottleManagerAccess(
    ExtensionThrottleManager* manager)
    : manager_(manager) {
  CHECK(manager_);
}

ExtensionThrottleManagerAccess::~ExtensionThrottleManagerAccess() = default;

void ExtensionThrottleManagerAccess::SetDestroyed() {
  base::AutoLock auto_lock(lock_);
  manager_ = nullptr;
}

std::tuple<base::MovableAutoLock, ExtensionThrottleManager*>
ExtensionThrottleManagerAccess::Get() {
  base::MovableAutoLock auto_lock(lock_);
  return std::make_tuple(std::move(auto_lock), manager_);
}

}  // namespace extensions
