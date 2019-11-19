// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_LOCATION_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_LOCATION_H_

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include "base/macros.h"

namespace device {
class WMRInputLocation {
 public:
  virtual ~WMRInputLocation() = default;

  virtual bool TryGetPosition(
      ABI::Windows::Foundation::Numerics::Vector3* position) const = 0;

  virtual bool TryGetOrientation(
      ABI::Windows::Foundation::Numerics::Quaternion* orientation) const = 0;

  virtual bool TryGetPositionAccuracy(
      ABI::Windows::UI::Input::Spatial::
          SpatialInteractionSourcePositionAccuracy* position_accuracy)
      const = 0;
};

class WMRInputLocationImpl : public WMRInputLocation {
 public:
  explicit WMRInputLocationImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation>
          location);
  ~WMRInputLocationImpl() override;

  // Uses ISpatialInteractionSourceLocation.
  bool TryGetPosition(
      ABI::Windows::Foundation::Numerics::Vector3* position) const override;

  // Uses ISpatialInteractionSourceLocation2.
  bool TryGetOrientation(ABI::Windows::Foundation::Numerics::Quaternion*
                             orientation) const override;

  // Uses ISpatialInteractionSourceLocation3.
  bool TryGetPositionAccuracy(ABI::Windows::UI::Input::Spatial::
                                  SpatialInteractionSourcePositionAccuracy*
                                      position_accuracy) const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation>
      location_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation2>
      location2_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation3>
      location3_;

  DISALLOW_COPY(WMRInputLocationImpl);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_LOCATION_H_
