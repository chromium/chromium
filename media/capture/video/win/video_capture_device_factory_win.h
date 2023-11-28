// Copyright 2014 The Chromium Authors
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

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

CAPTURE_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3D11VideoCaptureBlocklist);

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Devices::Enumeration::DeviceInformationCollection;

enum class MFSourceOutcome {
  kSuccess = 0,
  // Failed due to an unknown or unspecified reason.
  kFailed,
  // Failed to open due to OS-level system permissions.
  kFailedSystemPermissions,
};

// Extension of VideoCaptureDeviceFactory to create and manipulate Windows
// devices, via either DirectShow or MediaFoundation APIs.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryWin
    : public VideoCaptureDeviceFactory {
 public:
  class ComThreadData;
  static bool PlatformSupportsMediaFoundation();

  VideoCaptureDeviceFactoryWin();

  VideoCaptureDeviceFactoryWin(const VideoCaptureDeviceFactoryWin&) = delete;
  VideoCaptureDeviceFactoryWin& operator=(const VideoCaptureDeviceFactoryWin&) =
      delete;

  ~VideoCaptureDeviceFactoryWin() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

  void set_use_media_foundation_for_testing(bool use) {
    use_media_foundation_ = use;
  }

  void set_use_d3d11_with_media_foundation_for_testing(bool use) {
    use_d3d11_with_media_foundation_ = use;
  }

  scoped_refptr<DXGIDeviceManager> GetDxgiDeviceManager() override;

 protected:
  // Protected and virtual for testing.
  virtual bool CreateDeviceEnumMonikerDirectShow(IEnumMoniker** enum_moniker);
  virtual bool CreateDeviceFilterDirectShow(const std::string& device_id,
                                            IBaseFilter** capture_filter);
  virtual bool CreateDeviceFilterDirectShow(
      Microsoft::WRL::ComPtr<IMoniker> moniker,
      IBaseFilter** capture_filter);
  virtual MFSourceOutcome CreateDeviceSourceMediaFoundation(
      const std::string& device_id,
      VideoCaptureApi capture_api,
      const bool banned_for_d3d11,
      IMFMediaSource** source_out);
  virtual MFSourceOutcome CreateDeviceSourceMediaFoundation(
      Microsoft::WRL::ComPtr<IMFAttributes> attributes,
      const bool banned_for_d3d11,
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
      const bool banned_for_d3d11,
      const std::string& display_name);

  bool use_d3d11_with_media_foundation_for_testing() {
    return use_d3d11_with_media_foundation_;
  }

  void OnGpuInfoUpdate(const CHROME_LUID& luid) override;

 private:
  void DeviceInfoReady(std::vector<VideoCaptureDeviceInfo> devices_info,
                       GetDevicesInfoCallback result_callback);
  std::vector<VideoCaptureDeviceInfo> GetDevicesInfoMediaFoundation();
  void AugmentDevicesListWithDirectShowOnlyDevices(
      std::vector<VideoCaptureDeviceInfo>* devices_info);
  // Queries DirectShow devices, skips over devices listed in |known_devices|
  // with non-empty supported formats.
  std::vector<VideoCaptureDeviceInfo> GetDevicesInfoDirectShow(
      const std::vector<VideoCaptureDeviceInfo>& known_devices);

  void UpdateDevicesInfoAvailability(
      std::vector<VideoCaptureDeviceInfo>* devices_info);
  void CreateUsageMonitorAndReportHandler();

  bool use_media_foundation_;
  bool use_d3d11_with_media_foundation_;

  // Preferred adapter to use.
  CHROME_LUID luid_ = {0, 0};

  // For calling WinRT methods on a COM initiated thread.
  base::Thread com_thread_;
  scoped_refptr<ComThreadData> com_thread_data_;
  // For hardware acceleration in MediaFoundation capture engine
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;

  class UsageReportHandler;
  scoped_refptr<UsageReportHandler> report_handler_;
  Microsoft::WRL::ComPtr<IMFSensorActivityMonitor> monitor_;

  base::WeakPtrFactory<VideoCaptureDeviceFactoryWin> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_FACTORY_WIN_H_
