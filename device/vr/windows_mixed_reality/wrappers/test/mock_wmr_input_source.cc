// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_source.h"

namespace device {

uint16_t MockWMRController::ProductId() {
  return 0;
}

uint16_t MockWMRController::VendorId() {
  return 0;
}

MockWMRInputSource::MockWMRInputSource(ControllerFrameData data,
                                       unsigned int id)
    : data_(data), id_(id) {}

MockWMRInputSource::~MockWMRInputSource() = default;

uint32_t MockWMRInputSource::Id() const {
  return id_;
}

ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind
MockWMRInputSource::Kind() const {
  switch (data_.role) {
    case ControllerRole::kControllerRoleLeft:
    case ControllerRole::kControllerRoleRight:
      return ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourceKind_Controller;
    case ControllerRole::kControllerRoleVoice:
      return ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourceKind_Voice;
    case ControllerRole::kControllerRoleInvalid:
      NOTREACHED();
      return ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourceKind_Controller;
  }
}

bool MockWMRInputSource::IsPointingSupported() const {
  return true;
}

std::unique_ptr<WMRController> MockWMRInputSource::Controller() const {
  return std::make_unique<MockWMRController>();
}

ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness
MockWMRInputSource::Handedness() const {
  switch (data_.role) {
    case ControllerRole::kControllerRoleLeft:
      return ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourceHandedness_Left;
    case ControllerRole::kControllerRoleRight:
      return ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourceHandedness_Right;
    default:
      return ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourceHandedness_Unspecified;
  }
}

}  // namespace device
