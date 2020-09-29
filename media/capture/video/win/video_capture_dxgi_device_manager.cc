// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_dxgi_device_manager.h"

#include <d3d11.h>
#include <mfcaptureengine.h>
#include "base/logging.h"

using Microsoft::WRL::ComPtr;

namespace media {

scoped_refptr<VideoCaptureDXGIDeviceManager>
VideoCaptureDXGIDeviceManager::Create() {
  ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager;
  UINT d3d_device_reset_token = 0;
  HRESULT hr = MFCreateDXGIDeviceManager(&d3d_device_reset_token,
                                         &mf_dxgi_device_manager);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create MF DXGI device manager: "
                << logging::SystemErrorCodeToString(hr);
    return scoped_refptr<VideoCaptureDXGIDeviceManager>();
  }
  scoped_refptr<VideoCaptureDXGIDeviceManager>
      video_capture_dxgi_device_manager(new VideoCaptureDXGIDeviceManager(
          std::move(mf_dxgi_device_manager), d3d_device_reset_token));
  if (!video_capture_dxgi_device_manager->ResetDevice()) {
    // If setting a device failed, ensure that an empty scoped_refptr is
    // returned so that we fall back to software mode
    return scoped_refptr<VideoCaptureDXGIDeviceManager>();
  }
  return video_capture_dxgi_device_manager;
}

VideoCaptureDXGIDeviceManager::VideoCaptureDXGIDeviceManager(
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager,
    UINT d3d_device_reset_token)
    : mf_dxgi_device_manager_(std::move(mf_dxgi_device_manager)),
      d3d_device_reset_token_(d3d_device_reset_token) {}

VideoCaptureDXGIDeviceManager::~VideoCaptureDXGIDeviceManager() {}

bool VideoCaptureDXGIDeviceManager::ResetDevice() {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device;
  constexpr uint32_t device_flags =
      (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT);
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                 device_flags, nullptr, 0, D3D11_SDK_VERSION,
                                 &d3d_device, nullptr, nullptr);
  if (FAILED(hr)) {
    DLOG(ERROR) << "D3D11 device creation failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }
  hr = mf_dxgi_device_manager_->ResetDevice(d3d_device.Get(),
                                            d3d_device_reset_token_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to reset device on MF DXGI device manager: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

void VideoCaptureDXGIDeviceManager::RegisterInCaptureEngineAttributes(
    IMFAttributes* attributes) {
  HRESULT result = attributes->SetUnknown(MF_CAPTURE_ENGINE_D3D_MANAGER,
                                          mf_dxgi_device_manager_.Get());
  DCHECK(SUCCEEDED(result));
}

void VideoCaptureDXGIDeviceManager::RegisterWithMediaSource(
    ComPtr<IMFMediaSource> media_source) {
  ComPtr<IMFMediaSourceEx> source_ext;
  if (FAILED(media_source.As(&source_ext))) {
    DCHECK(false);
    return;
  }
  source_ext->SetD3DManager(mf_dxgi_device_manager_.Get());
}

}  // namespace media