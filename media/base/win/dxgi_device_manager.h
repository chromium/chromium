// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_DXGI_DEVICE_MANAGER_H_
#define MEDIA_BASE_WIN_DXGI_DEVICE_MANAGER_H_

#include <d3d11.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"

namespace media {

// Wrap around the usage of device handle from |device_manager|.
class MEDIA_EXPORT DXGIDeviceScopedHandle {
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

class MEDIA_EXPORT DXGIDeviceManager
    : public base::RefCountedThreadSafe<DXGIDeviceManager> {
 public:
  DXGIDeviceManager(const DXGIDeviceManager&) = delete;
  DXGIDeviceManager& operator=(const DXGIDeviceManager&) = delete;

  // Returns a DXGIDeviceManager with associated D3D device set, or nullptr on
  // failure.
  static scoped_refptr<DXGIDeviceManager> Create(CHROME_LUID luid);

  // Associates a new D3D device with the DXGI Device Manager
  // returns it in the parameter, which can't be nullptr.
  virtual HRESULT ResetDevice(Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device);

  // Checks if the local device was removed, recreates it if needed.
  // Returns DeviceRemovedReason HRESULT value.
  // Returns the local device in |new_device|, if it's not nullptr.
  virtual HRESULT CheckDeviceRemovedAndGetDevice(
      Microsoft::WRL::ComPtr<ID3D11Device>* new_device);

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

  void OnGpuInfoUpdate(CHROME_LUID luid);

 protected:
  friend class base::RefCountedThreadSafe<DXGIDeviceManager>;
  DXGIDeviceManager(
      Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager,
      UINT d3d_device_reset_token,
      CHROME_LUID luid);
  virtual ~DXGIDeviceManager();

  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> mf_dxgi_device_manager_;
  UINT d3d_device_reset_token_ = 0;
  CHROME_LUID luid_ = {0, 0};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_DXGI_DEVICE_MANAGER_H_
