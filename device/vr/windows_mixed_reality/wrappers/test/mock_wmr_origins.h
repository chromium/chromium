// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_ORIGINS_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_ORIGINS_H_

#include <windows.perception.spatial.h>
#include <wrl.h>

#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"

namespace device {

class MockWMRCoordinateSystem : public WMRCoordinateSystem {
 public:
  MockWMRCoordinateSystem();
  ~MockWMRCoordinateSystem() override;
  bool TryGetTransformTo(
      const WMRCoordinateSystem* other,
      ABI::Windows::Foundation::Numerics::Matrix4x4* this_to_other) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRCoordinateSystem);
};

class MockWMRStationaryOrigin : public WMRStationaryOrigin {
 public:
  MockWMRStationaryOrigin();
  ~MockWMRStationaryOrigin() override;

  std::unique_ptr<WMRCoordinateSystem> CoordinateSystem() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRStationaryOrigin);
};

class MockWMRAttachedOrigin : public WMRAttachedOrigin {
 public:
  MockWMRAttachedOrigin();
  ~MockWMRAttachedOrigin() override;

  std::unique_ptr<WMRCoordinateSystem> TryGetCoordinatesAtTimestamp(
      const WMRTimestamp* timestamp) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRAttachedOrigin);
};

class MockWMRStageOrigin : public WMRStageOrigin {
 public:
  MockWMRStageOrigin();
  ~MockWMRStageOrigin() override;

  std::unique_ptr<WMRCoordinateSystem> CoordinateSystem() override;
  ABI::Windows::Perception::Spatial::SpatialMovementRange MovementRange()
      override;
  std::vector<ABI::Windows::Foundation::Numerics::Vector3> GetMovementBounds(
      const WMRCoordinateSystem* coordinates) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRStageOrigin);
};

class MockWMRStageStatics : public WMRStageStatics {
 public:
  MockWMRStageStatics();
  ~MockWMRStageStatics() override;

  std::unique_ptr<WMRStageOrigin> CurrentStage() override;

  std::unique_ptr<base::CallbackList<void()>::Subscription>
  AddStageChangedCallback(const base::RepeatingCallback<void()>& cb) override;

 private:
  base::CallbackList<void()> callback_list_;

  DISALLOW_COPY_AND_ASSIGN(MockWMRStageStatics);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_ORIGINS_H_
