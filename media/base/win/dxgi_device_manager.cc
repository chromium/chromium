// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/dxgi_device_manager.h"

#include <mfcaptureengine.h>
#include <mferror.h>
#include <mfreadwrite.h>

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"

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

scoped_refptr<DXGIDeviceManager> DXGIDeviceManager::Create(CHROME_LUID luid) {
  if (!InitializeMediaFoundation()) {
    DLOG(ERROR) << "MF DXGI Device Manager is not available";
    return nullptr;
  }
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager;
  UINT d3d_device_reset_token = 0;
  HRESULT hr = MFCreateDXGIDeviceManager(&d3d_device_reset_token,
                                         &mf_dxgi_device_manager);
  RETURN_ON_HR_FAILURE(hr, "Failed to create MF DXGI device manager", nullptr);
  auto dxgi_device_manager = base::WrapRefCounted(new DXGIDeviceManager(
      std::move(mf_dxgi_device_manager), d3d_device_reset_token, luid));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device;
  if (dxgi_device_manager &&
      FAILED(dxgi_device_manager->ResetDevice(d3d_device))) {
    // If setting a device failed, ensure that an empty scoped_refptr is
    // returned as the dxgi_device_manager is not usable without a device.
    return nullptr;
  }
  return dxgi_device_manager;
}

DXGIDeviceManager::DXGIDeviceManager(
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager,
    UINT d3d_device_reset_token,
    CHROME_LUID luid)
    : mf_dxgi_device_manager_(std::move(mf_dxgi_device_manager)),
      d3d_device_reset_token_(d3d_device_reset_token),
      luid_(luid) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DXGIDeviceManager::~DXGIDeviceManager() = default;

HRESULT DXGIDeviceManager::ResetDevice(
    Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr uint32_t kDeviceFlags =
      D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  const D3D_FEATURE_LEVEL kFeatureLevels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0};

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;

  if (luid_.HighPart || luid_.LowPart) {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    RETURN_ON_HR_FAILURE(hr, "Failed to create DXGIFactory1", hr);
    Microsoft::WRL::ComPtr<IDXGIAdapter> temp;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters(i, &temp)); i++) {
      DXGI_ADAPTER_DESC desc;
      if (SUCCEEDED(temp->GetDesc(&desc)) &&
          desc.AdapterLuid.HighPart == luid_.HighPart &&
          desc.AdapterLuid.LowPart == luid_.LowPart) {
        adapter = temp;
        break;
      }
    }
  }
  // If adapter is not nullptr, the driver type must be D3D_DRIVER_TYPE_UNKNOWN
  // or D3D11CreateDevice will return E_INVALIDARG.
  HRESULT hr = D3D11CreateDevice(
      adapter.Get(),
      adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
      kDeviceFlags, kFeatureLevels, std::size(kFeatureLevels),
      D3D11_SDK_VERSION, &d3d_device, nullptr, nullptr);
  RETURN_ON_HR_FAILURE(hr, "D3D11 device creation failed", hr);
  RETURN_ON_HR_FAILURE(
      hr, media::SetDebugName(d3d_device.Get(), "Media_DXGIDeviceManager"), hr);
  // Since FrameServerClient background threads in the video capture process
  // call EnqueueSetEvent on Chromium's D3D11 device at the same time that
  // Chromium is actively using it in a worker thread, we need to protect access
  // via ID3D10Multithreaded::SetMultithreadedProtect. Unfortunately, leaving
  // off the CREATE_DEVICE_SINGLETHREADED creation flag is not enough to protect
  // us.
  Microsoft::WRL::ComPtr<ID3D10Multithread> d3d_device_multithread;
  RETURN_IF_FAILED(d3d_device.As(&d3d_device_multithread));
  RETURN_IF_FAILED(d3d_device_multithread->SetMultithreadProtected(TRUE));
  hr = mf_dxgi_device_manager_->ResetDevice(d3d_device.Get(),
                                            d3d_device_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device on MF DXGI device manager",
                       hr);
  return S_OK;
}

HRESULT DXGIDeviceManager::CheckDeviceRemovedAndGetDevice(
    Microsoft::WRL::ComPtr<ID3D11Device>* new_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Microsoft::WRL::ComPtr<ID3D11Device> device = GetDevice();
  HRESULT hr = device ? device->GetDeviceRemovedReason() : MF_E_UNEXPECTED;
  if (FAILED(hr)) {
    HRESULT reset_hr = ResetDevice(device);
    if (FAILED(reset_hr)) {
      LOG(ERROR) << "Failed to recreate the device: "
                 << logging::SystemErrorCodeToString(reset_hr);
      if (new_device) {
        *new_device = nullptr;
      }
      return hr;
    }
  }
  if (new_device) {
    *new_device = std::move(device);
  }
  return hr;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DXGIDeviceScopedHandle device_handle(mf_dxgi_device_manager_.Get());
  return device_handle.GetDevice();
}

Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>
DXGIDeviceManager::GetMFDXGIDeviceManager() {
  return mf_dxgi_device_manager_;
}

void DXGIDeviceManager::OnGpuInfoUpdate(CHROME_LUID luid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (luid.HighPart != luid_.HighPart || luid.LowPart != luid_.LowPart) {
    luid_ = luid;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    ResetDevice(device);
  }
}

}  // namespace media
