// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_RENDERING_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_RENDERING_H_

#include "device/vr/windows_mixed_reality/wrappers/wmr_rendering.h"

namespace device {

static constexpr int kDefaultWmrRenderWidth = 1440;
static constexpr int kDefaultWmrRenderHeight = 1600;

class MockWMRCamera : public WMRCamera {
 public:
  MockWMRCamera();
  ~MockWMRCamera() override;

  ABI::Windows::Foundation::Size RenderTargetSize() override;
  bool IsStereo() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRCamera);
};

class MockWMRCameraPose : public WMRCameraPose {
 public:
  MockWMRCameraPose();
  ~MockWMRCameraPose() override;

  ABI::Windows::Foundation::Rect Viewport() override;
  std::unique_ptr<WMRCamera> HolographicCamera() override;
  ABI::Windows::Graphics::Holographic::HolographicStereoTransform
  ProjectionTransform() override;
  bool TryGetViewTransform(
      const WMRCoordinateSystem* origin,
      ABI::Windows::Graphics::Holographic::HolographicStereoTransform*
          transform) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWMRCameraPose);
};

class MockWMRRenderingParameters : public WMRRenderingParameters {
 public:
  MockWMRRenderingParameters(
      const Microsoft::WRL::ComPtr<ID3D11Device>& device);
  ~MockWMRRenderingParameters() override;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> TryGetBackbufferAsTexture2D()
      override;

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer_texture_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(MockWMRRenderingParameters);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_RENDERING_H_
