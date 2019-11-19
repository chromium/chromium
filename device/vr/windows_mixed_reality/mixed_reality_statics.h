// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_STATICS_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_STATICS_H_

#include <memory>

#include "device/vr/test/locked_vr_test_hook.h"
#include "device/vr/vr_export.h"

namespace device {

class VRTestHook;

class DEVICE_VR_EXPORT MixedRealityDeviceStatics {
 public:
  static std::unique_ptr<MixedRealityDeviceStatics> CreateInstance();
  static void SetTestHook(VRTestHook* hook);
  static LockedVRTestHook GetLockedTestHook();
  static bool ShouldUseMocks();

  virtual ~MixedRealityDeviceStatics();

  // TODO(billorr): Consider notifications instead of polling.
  virtual bool IsHardwareAvailable() = 0;
  virtual bool IsApiAvailable() = 0;

 private:
  static VRTestHook* test_hook_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_STATICS_H_
