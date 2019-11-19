// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_STATE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_STATE_H_

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>
#include <memory>

namespace device {
class WMRCoordinateSystem;
class WMRInputLocation;
class WMRInputSource;
class WMRPointerPose;

class WMRInputSourceState {
 public:
  virtual ~WMRInputSourceState() = default;

  // Uses ISpatialInteractionSourceState.
  virtual std::unique_ptr<WMRPointerPose> TryGetPointerPose(
      const WMRCoordinateSystem* origin) const = 0;
  virtual std::unique_ptr<WMRInputSource> GetSource() const = 0;

  // Uses ISpatialInteractionSourceState2.
  virtual bool IsGrasped() const = 0;
  virtual bool IsSelectPressed() const = 0;
  virtual double SelectPressedValue() const = 0;

  virtual bool SupportsControllerProperties() const = 0;

  // Uses SpatialInteractionControllerProperties.
  virtual bool IsThumbstickPressed() const = 0;
  virtual bool IsTouchpadPressed() const = 0;
  virtual bool IsTouchpadTouched() const = 0;
  virtual double ThumbstickX() const = 0;
  virtual double ThumbstickY() const = 0;
  virtual double TouchpadX() const = 0;
  virtual double TouchpadY() const = 0;

  // Uses SpatialInteractionSourceProperties.
  virtual std::unique_ptr<WMRInputLocation> TryGetLocation(
      const WMRCoordinateSystem* origin) const = 0;
};

class WMRInputSourceStateImpl : public WMRInputSourceState {
 public:
  explicit WMRInputSourceStateImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState>
          source_state);
  WMRInputSourceStateImpl(const WMRInputSourceStateImpl& other);
  ~WMRInputSourceStateImpl() override;

  // Uses ISpatialInteractionSourceState.
  std::unique_ptr<WMRPointerPose> TryGetPointerPose(
      const WMRCoordinateSystem* origin) const override;
  std::unique_ptr<WMRInputSource> GetSource() const override;

  // Uses ISpatialInteractionSourceState2.
  bool IsGrasped() const override;
  bool IsSelectPressed() const override;
  double SelectPressedValue() const override;

  bool SupportsControllerProperties() const override;

  // Uses SpatialInteractionControllerProperties.
  bool IsThumbstickPressed() const override;
  bool IsTouchpadPressed() const override;
  bool IsTouchpadTouched() const override;
  double ThumbstickX() const override;
  double ThumbstickY() const override;
  double TouchpadX() const override;
  double TouchpadY() const override;

  // Uses SpatialInteractionSourceProperties.
  std::unique_ptr<WMRInputLocation> TryGetLocation(
      const WMRCoordinateSystem* origin) const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState>
      source_state_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState2>
      source_state2_;

  // Typically we want to restrict each wrapper to one "COM" class, but this is
  // a Property on SpatialInteractionSourceState that is just a struct.
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionControllerProperties>
      controller_properties_;

  // We only use one method from this class, which is a property on our class.
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceProperties>
      properties_;
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_STATE_H_
