// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_impl.h"

#include <sstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/metrics/histogram_functions.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/producer.mojom.h"

namespace {

// Translates a set of device infos reported by a VideoCaptureSystem to a set
// of device infos that the video capture service exposes to its client.
// A translation is needed, because the actual video frames, on their way
// from the VideoCaptureSystem to clients of the Video Capture Service, will
// pass through an instance of VideoCaptureDeviceClient, which performs certain
// format conversions.
// TODO(chfremer): A cleaner design would be to have this translation
// happen in VideoCaptureDeviceClient, and talk only to VideoCaptureDeviceClient
// instead of VideoCaptureSystem.
static void TranslateDeviceInfos(
    video_capture::DeviceFactory::GetDeviceInfosCallback callback,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  std::vector<media::VideoCaptureDeviceInfo> translated_device_infos;
  for (const auto& device_info : device_infos) {
    media::VideoCaptureDeviceInfo translated_device_info;
    translated_device_info.descriptor = device_info.descriptor;
    for (const auto& format : device_info.supported_formats) {
      media::VideoCaptureFormat translated_format;
      if (format.pixel_format == media::PIXEL_FORMAT_Y16 ||
          format.pixel_format == media::PIXEL_FORMAT_NV12) {
        translated_format.pixel_format = format.pixel_format;
      } else {
        translated_format.pixel_format = media::PIXEL_FORMAT_I420;
      }
      translated_format.frame_size = format.frame_size;
      translated_format.frame_rate = format.frame_rate;
      if (base::Contains(translated_device_info.supported_formats,
                         translated_format)) {
        continue;
      }
      translated_device_info.supported_formats.push_back(translated_format);
    }
    // We explicitly need to include device infos for which there are zero
    // supported formats reported until https://crbug.com/792260 is resolved.
    translated_device_infos.push_back(translated_device_info);
  }
  std::move(callback).Run(translated_device_infos);
}

static void DiscardDeviceInfosAndCallContinuation(
    base::OnceClosure continuation,
    const std::vector<media::VideoCaptureDeviceInfo>&) {
  std::move(continuation).Run();
}

}  // anonymous namespace

namespace video_capture {

#if BUILDFLAG(IS_CHROMEOS_ASH)
DeviceFactoryImpl::DeviceFactoryImpl(
    std::unique_ptr<media::VideoCaptureSystem> capture_system,
    media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner)
    : capture_system_(std::move(capture_system)),
      jpeg_decoder_factory_callback_(std::move(jpeg_decoder_factory_callback)),
      jpeg_decoder_task_runner_(std::move(jpeg_decoder_task_runner)),
      collision_delay_timer_(FROM_HERE,
                             base::Seconds(3),
                             this,
                             &DeviceFactoryImpl::RecordCollision),
      has_called_get_device_infos_(false),
      weak_factory_(this) {}
#else
DeviceFactoryImpl::DeviceFactoryImpl(
    std::unique_ptr<media::VideoCaptureSystem> capture_system)
    : capture_system_(std::move(capture_system)),
      has_called_get_device_infos_(false) {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

DeviceFactoryImpl::~DeviceFactoryImpl() = default;

void DeviceFactoryImpl::GetDeviceInfos(GetDeviceInfosCallback callback) {
  capture_system_->GetDeviceInfosAsync(
      base::BindOnce(&TranslateDeviceInfos, std::move(callback)));
  has_called_get_device_infos_ = true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void DeviceFactoryImpl::RecordCollision() {
  base::UmaHistogramBoolean("ChromeOS.Camera.ConcurrentAccess", true);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void DeviceFactoryImpl::CreateDevice(const std::string& device_id,
                                     CreateDeviceCallback create_callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto active_device_iter = active_devices_by_id_.find(device_id);
  if (active_device_iter != active_devices_by_id_.end()) {
    // The requested device is already in use, this only happens when lacros and
    // ash tries to access the camera at the same time.
    // In this case, the second request will be rejected.
    DeviceInfo info{
        nullptr,
        media::VideoCaptureError::kVideoCaptureDeviceFactorySecondCreateDenied};
    std::move(create_callback).Run(std::move(info));
    collision_delay_timer_.Reset();
    return;
  }

  base::UmaHistogramBoolean("ChromeOS.Camera.ConcurrentAccess", false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto create_and_add_new_device_cb = base::BindOnce(
      &DeviceFactoryImpl::CreateAndAddNewDevice, weak_factory_.GetWeakPtr(),
      device_id, std::move(create_callback));

  if (has_called_get_device_infos_) {
    std::move(create_and_add_new_device_cb).Run();
    return;
  }

  capture_system_->GetDeviceInfosAsync(
      base::BindOnce(&DiscardDeviceInfosAndCallContinuation,
                     std::move(create_and_add_new_device_cb)));
  has_called_get_device_infos_ = true;
}

void DeviceFactoryImpl::StopDevice(const std::string device_id) {
  OnClientConnectionErrorOrClose(device_id);
}

void DeviceFactoryImpl::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  NOTIMPLEMENTED();
}

void DeviceFactoryImpl::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  NOTIMPLEMENTED();
}

void DeviceFactoryImpl::AddGpuMemoryBufferVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::GpuMemoryBufferVirtualDevice>
        virtual_device_receiver) {
  NOTIMPLEMENTED();
}

void DeviceFactoryImpl::RegisterVirtualDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
    bool raise_event_if_virtual_devices_already_present) {
  NOTIMPLEMENTED();
}

void DeviceFactoryImpl::CreateAndAddNewDevice(
    const std::string& device_id,
    CreateDeviceCallback create_callback) {
  media::VideoCaptureErrorOrDevice device_status =
      capture_system_->CreateDevice(device_id);
  if (!device_status.ok()) {
    DeviceInfo info{nullptr, device_status.error()};
    std::move(create_callback).Run(std::move(info));
    return;
  }

  // Add entry to active_devices to keep track of it.
  std::unique_ptr<DeviceMediaToMojoAdapter> device_entry;
  std::unique_ptr<media::VideoCaptureDevice> media_device =
      device_status.ReleaseDevice();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  device_entry = std::make_unique<DeviceMediaToMojoAdapter>(
      std::move(media_device), jpeg_decoder_factory_callback_,
      jpeg_decoder_task_runner_);
#elif BUILDFLAG(IS_WIN)  // BUILDFLAG(IS_CHROMEOS_ASH)
  device_entry = std::make_unique<DeviceMediaToMojoAdapter>(
      std::move(media_device), capture_system_->GetFactory());
#else                    // BUILDFLAG(IS_WIN)
  device_entry =
      std::make_unique<DeviceMediaToMojoAdapter>(std::move(media_device));
#endif                   // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_WIN)

  DeviceInfo info{device_entry.get(), media::VideoCaptureError::kNone};
  std::move(create_callback).Run(std::move(info));

  active_devices_by_id_[device_id] = std::move(device_entry);
}

void DeviceFactoryImpl::OnClientConnectionErrorOrClose(
    const std::string& device_id) {
  auto active_device_iter = active_devices_by_id_.find(device_id);
  if (active_device_iter != active_devices_by_id_.end()) {
    active_device_iter->second->Stop();
    active_devices_by_id_.erase(device_id);
  }
}

#if BUILDFLAG(IS_WIN)
void DeviceFactoryImpl::OnGpuInfoUpdate(const CHROME_LUID& luid) {
  capture_system_->GetFactory()->OnGpuInfoUpdate(luid);
}
#endif

}  // namespace video_capture
