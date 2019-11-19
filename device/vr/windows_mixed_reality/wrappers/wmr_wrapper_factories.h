// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_WRAPPER_FACTORIES_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_WRAPPER_FACTORIES_H_

#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_space.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_manager.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"

namespace device {

class WMRStationaryOriginFactory {
 public:
  static std::unique_ptr<WMRStationaryOrigin> CreateAtCurrentLocation();
};

class WMRAttachedOriginFactory {
 public:
  static std::unique_ptr<WMRAttachedOrigin> CreateAtCurrentLocation();
};

class WMRStageStaticsFactory {
 public:
  static std::unique_ptr<WMRStageStatics> Create();
};

class WMRSpatialAnchorFactory {
 public:
  static std::unique_ptr<WMRCoordinateSystem> TryCreateRelativeTo(
      WMRCoordinateSystem* origin);
};

class WMRInputManagerFactory {
 public:
  static std::unique_ptr<WMRInputManager> GetForWindow(HWND hwnd);
};

class WMRHolographicSpaceFactory {
 public:
  static std::unique_ptr<WMRHolographicSpace> CreateForWindow(HWND hwnd);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_WRAPPER_FACTORIES_H_
