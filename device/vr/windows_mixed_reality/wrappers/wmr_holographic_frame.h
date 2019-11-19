// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_

#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace device {
class WMRCameraPose;
class WMRRenderingParameters;
class WMRTimestamp;

class WMRHolographicFramePrediction {
 public:
  virtual ~WMRHolographicFramePrediction() = default;

  virtual std::unique_ptr<WMRTimestamp> Timestamp() = 0;
  virtual std::vector<std::unique_ptr<WMRCameraPose>> CameraPoses() = 0;
};

class WMRHolographicFramePredictionImpl : public WMRHolographicFramePrediction {
 public:
  explicit WMRHolographicFramePredictionImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicFramePrediction>
          prediction);
  ~WMRHolographicFramePredictionImpl() override;

  std::unique_ptr<WMRTimestamp> Timestamp() override;
  std::vector<std::unique_ptr<WMRCameraPose>> CameraPoses() override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicFramePrediction>
      prediction_;

  DISALLOW_COPY_AND_ASSIGN(WMRHolographicFramePredictionImpl);
};

class WMRHolographicFrame {
 public:
  virtual ~WMRHolographicFrame() = default;

  virtual std::unique_ptr<WMRHolographicFramePrediction>
  CurrentPrediction() = 0;
  virtual std::unique_ptr<WMRRenderingParameters> TryGetRenderingParameters(
      const WMRCameraPose* pose) = 0;
  virtual bool TryPresentUsingCurrentPrediction() = 0;
};

class WMRHolographicFrameImpl : public WMRHolographicFrame {
 public:
  explicit WMRHolographicFrameImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicFrame>
          holographic_frame);
  ~WMRHolographicFrameImpl() override;

  std::unique_ptr<WMRHolographicFramePrediction> CurrentPrediction() override;
  std::unique_ptr<WMRRenderingParameters> TryGetRenderingParameters(
      const WMRCameraPose* pose) override;
  bool TryPresentUsingCurrentPrediction() override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::IHolographicFrame>
      holographic_frame_;

  DISALLOW_COPY_AND_ASSIGN(WMRHolographicFrameImpl);
};

}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_HOLOGRAPHIC_FRAME_H_
