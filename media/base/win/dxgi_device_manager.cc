// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/dxgi_device_manager.h"

#include <mfcaptureengine.h>
#include <mfreadwrite.h>

#include "base/win/windows_version.h"
#include "media/base/win/mf_helpers.h"

namespace media {

DXGIDeviceScopedHandle::DXGIDeviceScopedHandle(
    IMFDXGIDeviceManager* device_manager)
    : device_manager_(device_manager) {}

DXGIDeviceScopedHandle::~DXGIDeviceScopedHandle() {
  if (device_handle_ == INVALID_HANDLE_VALUE) {
    return;
  }

  HRESULT hr = device_manager_->CloseDeviceHandle(device_handle_);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to close device handle";
  device_handle_ = INVALID_HANDLE_VALUE;
}

HRESULT DXGIDeviceScopedHandle::LockDevice(REFIID riid, void** device_out) {
  HRESULT hr = S_OK;
  if (device_handle_ == INVALID_HANDLE_VALUE) {
    hr = device_manager_->OpenDeviceHandle(&device_handle_);
    RETURN_ON_HR_FAILURE(
        hr, "Failed to open device handle on MF DXGI device manager", hr);
  }
  // see
  // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgidevicemanager-lockdevice
  // for details of LockDevice call.
  hr = device_manager_->LockDevice(device_handle_, riid, device_out,
                                   /*block=*/FALSE);
  return hr;
}

Microsoft::WRL::ComPtr<ID3D11Device> DXGIDeviceScopedHandle::GetDevice() {
  HRESULT hr = S_OK;
  if (device_handle_ == INVALID_HANDLE_VALUE) {
    hr = device_manager_->OpenDeviceHandle(&device_handle_);
    RETURN_ON_HR_FAILURE(
        hr, "Failed to open device handle on MF DXGI device manager", nullptr);
  }
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  hr = device_manager_->GetVideoService(device_handle_, IID_PPV_ARGS(&device));
  RETURN_ON_HR_FAILURE(hr, "Failed to get device from MF DXGI device manager",
                       nullptr);
  return device;
}

scoped_refptr<DXGIDeviceManager> DXGIDeviceManager::Create() {
  if (base::win::GetVersion() < base::win::Version::WIN8 ||
      (!::GetModuleHandle(L"mfplat.dll") && !::LoadLibrary(L"mfplat.dll"))) {
    // The MF DXGI Device manager is only supported on Win8 or later
    // Additionally, it is not supported when mfplat.dll isn't available
    DLOG(ERROR)
        << "MF DXGI Device Manager not supported on current version of Windows";
    return nullptr;
  }
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager;
  UINT d3d_device_reset_token = 0;
  HRESULT hr = MFCreateDXGIDeviceManager(&d3d_device_reset_token,
                                         &mf_dxgi_device_manager);
  RETURN_ON_HR_FAILURE(hr, "Failed to create MF DXGI device manager", nullptr);
  auto dxgi_device_manager = base::WrapRefCounted(new DXGIDeviceManager(
      std::move(mf_dxgi_device_manager), d3d_device_reset_token));
  if (dxgi_device_manager && FAILED(dxgi_device_manager->ResetDevice())) {
    // If setting a device failed, ensure that an empty scoped_refptr is
    // returned as the dxgi_device_manager is not usable without a device.
    return nullptr;
  }
  return dxgi_device_manager;
}

DXGIDeviceManager::DXGIDeviceManager(
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager,
    UINT d3d_device_reset_token)
    : mf_dxgi_device_manager_(std::move(mf_dxgi_device_manager)),
      d3d_device_reset_token_(d3d_device_reset_token) {}

DXGIDeviceManager::~DXGIDeviceManager() = default;

HRESULT DXGIDeviceManager::ResetDevice() {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device;
  constexpr uint32_t kDeviceFlags =
      D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                 kDeviceFlags, nullptr, 0, D3D11_SDK_VERSION,
                                 &d3d_device, nullptr, nullptr);
  RETURN_ON_HR_FAILURE(hr, "D3D11 device creation failed", hr);
  hr = mf_dxgi_device_manager_->ResetDevice(d3d_device.Get(),
                                            d3d_device_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device on MF DXGI device manager",
                       hr);
  return S_OK;
}

HRESULT DXGIDeviceManager::RegisterInCaptureEngineAttributes(
    IMFAttributes* attributes) {
  HRESULT hr = attributes->SetUnknown(MF_CAPTURE_ENGINE_D3D_MANAGER,
                                      mf_dxgi_device_manager_.Get());
  RETURN_ON_HR_FAILURE(
      hr, "Failed to set MF_CAPTURE_ENGINE_D3D_MANAGER attribute", hr);
  return S_OK;
}

HRESULT DXGIDeviceManager::RegisterInSourceReaderAttributes(
    IMFAttributes* attributes) {
  HRESULT hr = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER,
                                      mf_dxgi_device_manager_.Get());
  RETURN_ON_HR_FAILURE(
      hr, "Failed to set MF_SOURCE_READER_D3D_MANAGER attribute", hr);
  return S_OK;
}

HRESULT DXGIDeviceManager::RegisterWithMediaSource(
    Microsoft::WRL::ComPtr<IMFMediaSource> media_source) {
  Microsoft::WRL::ComPtr<IMFMediaSourceEx> source_ext;
  HRESULT hr = media_source.As(&source_ext);
  RETURN_ON_HR_FAILURE(hr, "Failed to query IMFMediaSourceEx", hr);
  hr = source_ext->SetD3DManager(mf_dxgi_device_manager_.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to set D3D manager", hr);
  return S_OK;
}

Microsoft::WRL::ComPtr<ID3D11Device> DXGIDeviceManager::GetDevice() {
  DXGIDeviceScopedHandle device_handle(mf_dxgi_device_manager_.Get());
  return device_handle.GetDevice();
}

Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>
DXGIDeviceManager::GetMFDXGIDeviceManager() {
  return mf_dxgi_device_manager_;
}

}  // namespace media
