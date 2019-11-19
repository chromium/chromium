// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_H_

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <cstdint>
#include <memory>

namespace device {
class WMRController {
 public:
  virtual ~WMRController() = default;

  virtual uint16_t ProductId() = 0;
  virtual uint16_t VendorId() = 0;
};

class WMRControllerImpl : public WMRController {
 public:
  explicit WMRControllerImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionController>
          controller);
  ~WMRControllerImpl() override;

  uint16_t ProductId() override;
  uint16_t VendorId() override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionController>
      controller_;
};

class WMRInputSource {
 public:
  virtual ~WMRInputSource() = default;

  // Uses ISpatialInteractionSource.
  virtual uint32_t Id() const = 0;
  virtual ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind Kind()
      const = 0;

  // Uses ISpatialInteractionSource2.
  virtual bool IsPointingSupported() const = 0;
  virtual std::unique_ptr<WMRController> Controller() const = 0;

  // Uses ISpatialInteractionSource3.
  virtual ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness
  Handedness() const = 0;

  virtual ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource*
  GetRawPtr() const;
};

class WMRInputSourceImpl : public WMRInputSource {
 public:
  explicit WMRInputSourceImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource> source);
  WMRInputSourceImpl(const WMRInputSourceImpl& other);
  ~WMRInputSourceImpl() override;

  // Uses ISpatialInteractionSource.
  uint32_t Id() const override;
  ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind Kind()
      const override;

  // Uses ISpatialInteractionSource2.
  bool IsPointingSupported() const override;
  std::unique_ptr<WMRController> Controller() const override;

  // Uses ISpatialInteractionSource3.
  ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness
  Handedness() const override;

  ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource* GetRawPtr()
      const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource>
      source_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource2>
      source2_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource3>
      source3_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_SOURCE_H_
