// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/locked_vr_test_hook.h"

namespace device {

// LockedVRTestHook
LockedVRTestHook::LockedVRTestHook(VRTestHook* test_hook)
    : test_hook_(test_hook) {
  LockProvider::GetLock()->Acquire();
}

LockedVRTestHook::~LockedVRTestHook() {
  LockProvider::GetLock()->Release();
}

VRTestHook* LockedVRTestHook::GetHook() {
  return test_hook_;
}

// LockProvider
LockProvider* LockProvider::instance_ = nullptr;

base::Lock* LockProvider::GetLock() {
  if (!instance_) {
    instance_ = new LockProvider();
  }
  return &(instance_->lock_);
}

LockProvider::LockProvider() : lock_() {
  DCHECK(!instance_);
}

LockProvider::~LockProvider() = default;

}  // namespace device
