// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_rendering.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "device/vr/windows/d3d11_texture_helper.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"

namespace WF = ABI::Windows::Foundation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface;
using ABI::Windows::Graphics::Holographic::HolographicStereoTransform;
using ABI::Windows::Graphics::Holographic::IHolographicCamera;
using ABI::Windows::Graphics::Holographic::IHolographicCameraPose;
using ABI::Windows::Graphics::Holographic::
    IHolographicCameraRenderingParameters;
using Microsoft::WRL::ComPtr;
using Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess;

namespace device {
// WMRCamera
WMRCameraImpl::WMRCameraImpl(ComPtr<IHolographicCamera> camera)
    : camera_(camera) {
  DCHECK(camera_);
}

WMRCameraImpl::~WMRCameraImpl() = default;

WF::Size WMRCameraImpl::RenderTargetSize() {
  WF::Size val;
  HRESULT hr = camera_->get_RenderTargetSize(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRCameraImpl::IsStereo() {
  boolean val;
  HRESULT hr = camera_->get_IsStereo(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

// WMRCameraPose
ABI::Windows::Graphics::Holographic::IHolographicCameraPose*
WMRCameraPose::GetRawPtr() const {
  // This should only ever be used by the real implementation, so by default
  // make sure it's not called.
  NOTREACHED();
  return nullptr;
}

WMRCameraPoseImpl::WMRCameraPoseImpl(ComPtr<IHolographicCameraPose> pose)
    : pose_(pose) {
  DCHECK(pose_);
}

WMRCameraPoseImpl::~WMRCameraPoseImpl() = default;

WF::Rect WMRCameraPoseImpl::Viewport() {
  WF::Rect val;
  HRESULT hr = pose_->get_Viewport(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

std::unique_ptr<WMRCamera> WMRCameraPoseImpl::HolographicCamera() {
  ComPtr<IHolographicCamera> camera;
  HRESULT hr = pose_->get_HolographicCamera(&camera);
  DCHECK(SUCCEEDED(hr));
  return std::make_unique<WMRCameraImpl>(camera);
}

HolographicStereoTransform WMRCameraPoseImpl::ProjectionTransform() {
  HolographicStereoTransform val;
  HRESULT hr = pose_->get_ProjectionTransform(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

bool WMRCameraPoseImpl::TryGetViewTransform(
    const WMRCoordinateSystem* origin,
    HolographicStereoTransform* transform) {
  ComPtr<IReference<HolographicStereoTransform>> transform_ref;
  if (FAILED(pose_->TryGetViewTransform(origin->GetRawPtr(), &transform_ref)) ||
      !transform_ref)
    return false;

  HRESULT hr = transform_ref->get_Value(transform);
  return SUCCEEDED(hr);
}

IHolographicCameraPose* WMRCameraPoseImpl::GetRawPtr() const {
  return pose_.Get();
}

// WMRRenderingParameters
WMRRenderingParametersImpl::WMRRenderingParametersImpl(
    ComPtr<IHolographicCameraRenderingParameters> rendering_params)
    : rendering_params_(rendering_params) {
  DCHECK(rendering_params_);
}

WMRRenderingParametersImpl::~WMRRenderingParametersImpl() = default;

ComPtr<ID3D11Texture2D>
WMRRenderingParametersImpl::TryGetBackbufferAsTexture2D() {
  ComPtr<IDirect3DSurface> surface;
  if (FAILED(rendering_params_->get_Direct3D11BackBuffer(&surface)))
    return nullptr;

  ComPtr<IDirect3DDxgiInterfaceAccess> dxgi_interface_access;
  if (FAILED(surface.As(&dxgi_interface_access)))
    return nullptr;

  ComPtr<ID3D11Resource> native_resource;
  if (FAILED(
          dxgi_interface_access->GetInterface(IID_PPV_ARGS(&native_resource))))
    return nullptr;

  ComPtr<ID3D11Texture2D> texture;
  if (FAILED(native_resource.As(&texture)))
    return nullptr;

  return texture;
}
}  // namespace device
