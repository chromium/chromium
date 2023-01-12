// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "services/video_capture/device.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace video_capture {

class DeviceFactory {
 public:
  struct DeviceInfo {
    raw_ptr<Device> device;
    media::VideoCaptureError result_code;
  };
  using CreateDeviceCallback = base::OnceCallback<void(DeviceInfo)>;
  using GetDeviceInfosCallback = base::OnceCallback<void(
      const std::vector<media::VideoCaptureDeviceInfo>&)>;

  virtual void GetDeviceInfos(GetDeviceInfosCallback callback) = 0;

  virtual void CreateDevice(const std::string& device_id,
                            CreateDeviceCallback callback) = 0;

  virtual void StopDevice(const std::string device_id) = 0;

  virtual void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<mojom::Producer> producer,
      mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) = 0;

  virtual void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::TextureVirtualDevice>
          virtual_device_receiver) = 0;

  virtual void AddGpuMemoryBufferVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
          virtual_device_receiver) = 0;

  virtual void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
      bool raise_event_if_virtual_devices_already_present) = 0;

#if BUILDFLAG(IS_WIN)
  virtual void OnGpuInfoUpdate(const CHROME_LUID& luid) = 0;
#endif

  virtual ~DeviceFactory() = default;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
