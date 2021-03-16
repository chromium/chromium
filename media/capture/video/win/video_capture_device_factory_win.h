// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactory class for Windows platforms.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_

#include <d3d11.h>
// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <mfidl.h>
#include <windows.devices.enumeration.h>
#include <wrl.h>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/base/win/mf_initializer.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Devices::Enumeration::DeviceInformationCollection;

// Extension of VideoCaptureDeviceFactory to create and manipulate Windows
// devices, via either DirectShow or MediaFoundation APIs.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWin
    : public VideoCaptureDeviceFactory {
 public:
  static bool PlatformSupportsMediaFoundation();

  VideoCaptureDeviceFactoryWin();
  ~VideoCaptureDeviceFactoryWin() override;

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

  void set_use_media_foundation_for_testing(bool use) {
    use_media_foundation_ = use;
  }

  void set_use_d3d11_with_media_foundation_for_testing(bool use) {
    use_d3d11_with_media_foundation_ = use;
  }

 protected:
  // Protected and virtual for testing.
  virtual bool CreateDeviceEnumMonikerDirectShow(IEnumMoniker** enum_moniker);
  virtual bool CreateDeviceFilterDirectShow(const std::string& device_id,
                                            IBaseFilter** capture_filter);
  virtual bool CreateDeviceFilterDirectShow(
      Microsoft::WRL::ComPtr<IMoniker> moniker,
      IBaseFilter** capture_filter);
  virtual bool CreateDeviceSourceMediaFoundation(const std::string& device_id,
                                                 VideoCaptureApi capture_api,
                                                 IMFMediaSource** source_out);
  virtual bool CreateDeviceSourceMediaFoundation(
      Microsoft::WRL::ComPtr<IMFAttributes> attributes,
      IMFMediaSource** source);
  virtual bool EnumerateDeviceSourcesMediaFoundation(
      Microsoft::WRL::ComPtr<IMFAttributes> attributes,
      IMFActivate*** devices,
      UINT32* count);
  virtual VideoCaptureFormats GetSupportedFormatsDirectShow(
      Microsoft::WRL::ComPtr<IBaseFilter> capture_filter,
      const std::string& display_name);
  virtual VideoCaptureFormats GetSupportedFormatsMediaFoundation(
      Microsoft::WRL::ComPtr<IMFMediaSource> source,
      const std::string& display_name);

  bool use_d3d11_with_media_foundation_for_testing() {
    return use_d3d11_with_media_foundation_;
  }

  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_for_testing() {
    return dxgi_device_manager_;
  }

 private:
  void EnumerateDevicesUWP(std::vector<VideoCaptureDeviceInfo> devices_info,
                           GetDevicesInfoCallback result_callback);
  void FoundAllDevicesUWP(
      std::vector<VideoCaptureDeviceInfo> devices_info,
      GetDevicesInfoCallback result_callback,
      IAsyncOperation<DeviceInformationCollection*>* operation);
  void DeviceInfoReady(std::vector<VideoCaptureDeviceInfo> devices_info,
                       GetDevicesInfoCallback result_callback);
  std::vector<VideoCaptureDeviceInfo> GetDevicesInfoMediaFoundation();
  void AugmentDevicesListWithDirectShowOnlyDevices(
      std::vector<VideoCaptureDeviceInfo>* devices_info);
  std::vector<VideoCaptureDeviceInfo> GetDevicesInfoDirectShow();

  bool use_media_foundation_;
  bool use_d3d11_with_media_foundation_;
  MFSessionLifetime session_;

  // For calling WinRT methods on a COM initiated thread.
  base::Thread com_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  std::unordered_set<IAsyncOperation<DeviceInformationCollection*>*> async_ops_;
  // For hardware acceleration in MediaFoundation capture engine
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  base::WeakPtrFactory<VideoCaptureDeviceFactoryWin> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
