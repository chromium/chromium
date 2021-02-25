// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_DXGI_DEVICE_MANAGER_H_
#define MEDIA_BASE_WIN_DXGI_DEVICE_MANAGER_H_

#include <d3d11.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/win/mf_initializer_export.h"

namespace media {

// Wrap around the usage of device handle from |device_manager|.
class MF_INITIALIZER_EXPORT DXGIDeviceScopedHandle {
 public:
  explicit DXGIDeviceScopedHandle(IMFDXGIDeviceManager* device_manager);
  DXGIDeviceScopedHandle(const DXGIDeviceScopedHandle&) = delete;
  DXGIDeviceScopedHandle& operator=(const DXGIDeviceScopedHandle&) = delete;
  ~DXGIDeviceScopedHandle();

  HRESULT LockDevice(REFIID riid, void** device_out);
  Microsoft::WRL::ComPtr<ID3D11Device> GetDevice();

 private:
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> device_manager_;

  HANDLE device_handle_ = INVALID_HANDLE_VALUE;
};

class MF_INITIALIZER_EXPORT DXGIDeviceManager
    : public base::RefCounted<DXGIDeviceManager> {
 public:
  DXGIDeviceManager(const DXGIDeviceManager&) = delete;
  DXGIDeviceManager& operator=(const DXGIDeviceManager&) = delete;

  // Returns a DXGIDeviceManager with associated D3D device set, or nullptr on
  // failure.
  static scoped_refptr<DXGIDeviceManager> Create();

  // Associates a new D3D device with the DXGI Device Manager
  virtual HRESULT ResetDevice();

  // Registers this manager in capture engine attributes.
  HRESULT RegisterInCaptureEngineAttributes(IMFAttributes* attributes);

  // Registers this manager in source reader attributes.
  HRESULT RegisterInSourceReaderAttributes(IMFAttributes* attributes);

  // Registers this manager with a media source
  HRESULT RegisterWithMediaSource(
      Microsoft::WRL::ComPtr<IMFMediaSource> media_source);

  // Directly access D3D device stored in DXGI device manager
  virtual Microsoft::WRL::ComPtr<ID3D11Device> GetDevice();

  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> GetMFDXGIDeviceManager();

 protected:
  friend class base::RefCounted<DXGIDeviceManager>;
  DXGIDeviceManager(
      Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager,
      UINT d3d_device_reset_token);
  virtual ~DXGIDeviceManager();

  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager_;
  UINT d3d_device_reset_token_ = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_DXGI_DEVICE_MANAGER_H_
