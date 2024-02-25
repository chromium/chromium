// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_

#include <map>

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_system.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace video_capture {

class DeviceMediaToMojoAdapter;

// Wraps a media::VideoCaptureSystem and exposes its functionality through the
// mojom::DeviceFactory interface. Keeps track of device instances that have
// been created to ensure that it does not create more than one instance of the
// same media::VideoCaptureDevice at the same time.
class DeviceFactoryImpl : public DeviceFactory {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DeviceFactoryImpl(
      std::unique_ptr<media::VideoCaptureSystem> capture_system,
      media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner);
#else
  DeviceFactoryImpl(std::unique_ptr<media::VideoCaptureSystem> capture_system);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DeviceFactoryImpl(const DeviceFactoryImpl&) = delete;
  DeviceFactoryImpl& operator=(const DeviceFactoryImpl&) = delete;

  ~DeviceFactoryImpl() override;

  // DeviceFactory implementation.
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    CreateDeviceCallback callback) override;
  void StopDevice(const std::string device_id) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<mojom::Producer> producer,
      mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::TextureVirtualDevice>
          virtual_device_receiver) override;
  void AddGpuMemoryBufferVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
          virtual_device_receiver) override;
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
      bool raise_event_if_virtual_devices_already_present) override;

#if BUILDFLAG(IS_WIN)
  void OnGpuInfoUpdate(const CHROME_LUID& luid) override;
#endif

 private:
  void CreateAndAddNewDevice(const std::string& device_id,
                             CreateDeviceCallback create_callback);

  void OnClientConnectionErrorOrClose(const std::string& device_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void RecordCollision();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const std::unique_ptr<media::VideoCaptureSystem> capture_system_;
  std::map<std::string, std::unique_ptr<DeviceMediaToMojoAdapter>>
      active_devices_by_id_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const media::MojoMjpegDecodeAcceleratorFactoryCB
      jpeg_decoder_factory_callback_;
  scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner_;
  base::DelayTimer collision_delay_timer_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  bool has_called_get_device_infos_;
  base::WeakPtrFactory<DeviceFactoryImpl> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
