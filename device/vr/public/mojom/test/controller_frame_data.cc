// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/mojom/test/controller_frame_data.h"

namespace device {

ControllerFrameData::ControllerFrameData() = default;
ControllerFrameData::~ControllerFrameData() = default;
ControllerFrameData::ControllerFrameData(ControllerFrameData&& other) = default;
ControllerFrameData& ControllerFrameData::operator=(
    ControllerFrameData&& other) = default;

ControllerFrameData::ControllerFrameData(const ControllerFrameData& other)
    : handedness(other.handedness),
      gamepad(other.gamepad ? other.gamepad.Clone() : nullptr),
      pose_data(other.pose_data),
      hand_data(other.hand_data ? other.hand_data.Clone() : nullptr),
      is_valid(other.is_valid) {}

ControllerFrameData& ControllerFrameData::operator=(
    const ControllerFrameData& other) {
  if (this != &other) {
    handedness = other.handedness;
    gamepad = other.gamepad ? other.gamepad.Clone() : nullptr;
    pose_data = other.pose_data;
    hand_data = other.hand_data ? other.hand_data.Clone() : nullptr;
    is_valid = other.is_valid;
  }
  return *this;
}
}  // namespace device
