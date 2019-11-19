// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_RENDERING_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_RENDERING_H_

#include <d3d11.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/macros.h"

namespace device {
class WMRCoordinateSystem;

class WMRCamera {
 public:
  virtual ~WMRCamera() = default;

  virtual ABI::Windows::Foundation::Size RenderTargetSize() = 0;
  virtual bool IsStereo() = 0;
};

class WMRCameraImpl : public WMRCamera {
 public:
  explicit WMRCameraImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicCamera> camera);
  ~WMRCameraImpl() override;

  ABI::Windows::Foundation::Size RenderTargetSize() override;
  bool IsStereo() override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCamera>
      camera_;

  DISALLOW_COPY_AND_ASSIGN(WMRCameraImpl);
};

class WMRCameraPose {
 public:
  virtual ~WMRCameraPose() = default;

  virtual ABI::Windows::Foundation::Rect Viewport() = 0;
  virtual std::unique_ptr<WMRCamera> HolographicCamera() = 0;
  virtual ABI::Windows::Graphics::Holographic::HolographicStereoTransform
  ProjectionTransform() = 0;
  virtual bool TryGetViewTransform(
      const WMRCoordinateSystem* origin,
      ABI::Windows::Graphics::Holographic::HolographicStereoTransform*
          transform) = 0;
  virtual ABI::Windows::Graphics::Holographic::IHolographicCameraPose*
  GetRawPtr() const;
};

class WMRCameraPoseImpl : public WMRCameraPose {
 public:
  explicit WMRCameraPoseImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Graphics::Holographic::IHolographicCameraPose> pose);
  ~WMRCameraPoseImpl() override;

  ABI::Windows::Foundation::Rect Viewport() override;
  std::unique_ptr<WMRCamera> HolographicCamera() override;
  ABI::Windows::Graphics::Holographic::HolographicStereoTransform
  ProjectionTransform() override;
  bool TryGetViewTransform(
      const WMRCoordinateSystem* origin,
      ABI::Windows::Graphics::Holographic::HolographicStereoTransform*
          transform) override;
  ABI::Windows::Graphics::Holographic::IHolographicCameraPose* GetRawPtr()
      const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Graphics::Holographic::IHolographicCameraPose>
      pose_;

  DISALLOW_COPY_AND_ASSIGN(WMRCameraPoseImpl);
};

class WMRRenderingParameters {
 public:
  virtual ~WMRRenderingParameters() = default;

  virtual Microsoft::WRL::ComPtr<ID3D11Texture2D>
  TryGetBackbufferAsTexture2D() = 0;
};

class WMRRenderingParametersImpl : public WMRRenderingParameters {
 public:
  explicit WMRRenderingParametersImpl(
      Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::
                                 IHolographicCameraRenderingParameters>
          rendering_params);
  ~WMRRenderingParametersImpl() override;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> TryGetBackbufferAsTexture2D()
      override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Holographic::
                             IHolographicCameraRenderingParameters>
      rendering_params_;

  DISALLOW_COPY_AND_ASSIGN(WMRRenderingParametersImpl);
};

}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_RENDERING_H_
