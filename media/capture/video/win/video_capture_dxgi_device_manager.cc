// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_dxgi_device_manager.h"

#include <d3d11.h>
#include <mfcaptureengine.h>
#include <mfreadwrite.h>
#include "base/logging.h"
#include "base/win/windows_version.h"

using Microsoft::WRL::ComPtr;

namespace media {

namespace {
class ScopedDeviceHandle {
 public:
  ScopedDeviceHandle(IMFDXGIDeviceManager* device_manager)
      : device_manager_(device_manager) {
    HRESULT hr = device_manager_->OpenDeviceHandle(&device_handle_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to open device handle on MF DXGI device manager: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }

  virtual ~ScopedDeviceHandle() {
    if (device_handle_) {
      device_manager_->CloseDeviceHandle(device_handle_);
    }
  }

  ComPtr<ID3D11Device> GetDevice() {
    if (!device_handle_) {
      return nullptr;
    }
    ComPtr<ID3D11Device> device;
    HRESULT hr =
        device_manager_->GetVideoService(device_handle_, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get device from MF DXGI device manager: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }
    return device;
  }

 private:
  ComPtr<IMFDXGIDeviceManager> device_manager_;
  HANDLE device_handle_ = nullptr;
};
}  // namespace

scoped_refptr<VideoCaptureDXGIDeviceManager>
VideoCaptureDXGIDeviceManager::Create() {
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    // The MF DXGI Device manager is only supported on Win8 or later
    DLOG(ERROR)
        << "MF DXGI Device Manager not supported on current version of Windows";
    return scoped_refptr<VideoCaptureDXGIDeviceManager>();
  }
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

void VideoCaptureDXGIDeviceManager::RegisterInSourceReaderAttributes(
    IMFAttributes* attributes) {
  HRESULT result = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER,
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

ComPtr<ID3D11Device> VideoCaptureDXGIDeviceManager::GetDevice() {
  ScopedDeviceHandle device_handle(mf_dxgi_device_manager_.Get());
  return device_handle.GetDevice();
}

}  // namespace media