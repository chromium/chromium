// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_FRAME_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_FRAME_H_

#include <d3d11.h>
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_frame.h"

namespace device {

struct SubmittedFrameData;

class MockWMRHolographicFramePrediction : public WMRHolographicFramePrediction {
 public:
  MockWMRHolographicFramePrediction();
  ~MockWMRHolographicFramePrediction() override;

  std::unique_ptr<WMRTimestamp> Timestamp() override;
  std::vector<std::unique_ptr<WMRCameraPose>> CameraPoses() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRHolographicFramePrediction);
};

class MockWMRHolographicFrame : public WMRHolographicFrame {
 public:
  MockWMRHolographicFrame(const Microsoft::WRL::ComPtr<ID3D11Device>& device);
  ~MockWMRHolographicFrame() override;

  std::unique_ptr<WMRHolographicFramePrediction> CurrentPrediction() override;
  std::unique_ptr<WMRRenderingParameters> TryGetRenderingParameters(
      const WMRCameraPose* pose) override;
  bool TryPresentUsingCurrentPrediction() override;

 private:
  bool CopyTextureDataIntoFrameData(SubmittedFrameData* data,
                                    unsigned int index);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer_texture_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(MockWMRHolographicFrame);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_HOLOGRAPHIC_FRAME_H_
