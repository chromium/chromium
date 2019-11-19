// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_origins.h"

namespace device {

// MockWMRCoordinateSystem
MockWMRCoordinateSystem::MockWMRCoordinateSystem() {}

MockWMRCoordinateSystem::~MockWMRCoordinateSystem() = default;

bool MockWMRCoordinateSystem::TryGetTransformTo(
    const WMRCoordinateSystem* other,
    ABI::Windows::Foundation::Numerics::Matrix4x4* this_to_other) {
  *this_to_other = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  return true;
}

// MockWMRStationaryOrigin
MockWMRStationaryOrigin::MockWMRStationaryOrigin() {}

MockWMRStationaryOrigin::~MockWMRStationaryOrigin() = default;

std::unique_ptr<WMRCoordinateSystem>
MockWMRStationaryOrigin::CoordinateSystem() {
  return std::make_unique<MockWMRCoordinateSystem>();
}

// MockWMRAttachedOrigin
MockWMRAttachedOrigin::MockWMRAttachedOrigin() {}

MockWMRAttachedOrigin::~MockWMRAttachedOrigin() = default;

std::unique_ptr<WMRCoordinateSystem>
MockWMRAttachedOrigin::TryGetCoordinatesAtTimestamp(
    const WMRTimestamp* timestamp) {
  return std::make_unique<MockWMRCoordinateSystem>();
}

// MockWMRStageOrigin
MockWMRStageOrigin::MockWMRStageOrigin() {}

MockWMRStageOrigin::~MockWMRStageOrigin() = default;

std::unique_ptr<WMRCoordinateSystem> MockWMRStageOrigin::CoordinateSystem() {
  return std::make_unique<MockWMRCoordinateSystem>();
}

ABI::Windows::Perception::Spatial::SpatialMovementRange
MockWMRStageOrigin::MovementRange() {
  return ABI::Windows::Perception::Spatial::SpatialMovementRange_NoMovement;
}

std::vector<ABI::Windows::Foundation::Numerics::Vector3>
MockWMRStageOrigin::GetMovementBounds(const WMRCoordinateSystem* coordinates) {
  return {};
}

// MockWMRStageStatics
MockWMRStageStatics::MockWMRStageStatics() {}

MockWMRStageStatics::~MockWMRStageStatics() = default;

std::unique_ptr<WMRStageOrigin> MockWMRStageStatics::CurrentStage() {
  return std::make_unique<MockWMRStageOrigin>();
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
MockWMRStageStatics::AddStageChangedCallback(
    const base::RepeatingCallback<void()>& cb) {
  return callback_list_.Add(cb);
}

}  // namespace device
