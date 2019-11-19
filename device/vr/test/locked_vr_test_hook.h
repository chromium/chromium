// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_LOCKED_VR_TEST_HOOK_H_
#define DEVICE_VR_TEST_LOCKED_VR_TEST_HOOK_H_

#include "base/synchronization/lock.h"

namespace device {

class VRTestHook;

// Provides thread safe access to a VRTestHook. Holds the lock as long as the
// object is alive.
class LockedVRTestHook {
 public:
  explicit LockedVRTestHook(VRTestHook* test_hook);
  ~LockedVRTestHook();

  VRTestHook* GetHook();

 private:
  VRTestHook* test_hook_;
};

// Singleton Lock holder. Workaround for not being able to have a static member
// lock due to not being able to re-assign.
class LockProvider {
 public:
  static base::Lock* GetLock();
  ~LockProvider();

 private:
  LockProvider();
  static LockProvider* instance_;
  base::Lock lock_;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_LOCKED_VR_TEST_HOOK_H_
