// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_media_to_mojo_adapter.h"

#include <sstream>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"

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
    video_capture::mojom::DeviceFactory::GetDeviceInfosCallback callback,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  std::vector<media::VideoCaptureDeviceInfo> translated_device_infos;
  for (const auto& device_info : device_infos) {
    media::VideoCaptureDeviceInfo translated_device_info;
    translated_device_info.descriptor = device_info.descriptor;
    for (const auto& format : device_info.supported_formats) {
      media::VideoCaptureFormat translated_format;
      translated_format.pixel_format =
          (format.pixel_format == media::PIXEL_FORMAT_Y16)
              ? media::PIXEL_FORMAT_Y16
              : media::PIXEL_FORMAT_I420;
      translated_format.frame_size = format.frame_size;
      translated_format.frame_rate = format.frame_rate;
      if (base::Contains(translated_device_info.supported_formats,
                         translated_format))
        continue;
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

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::ActiveDeviceEntry() =
    default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::~ActiveDeviceEntry() =
    default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::ActiveDeviceEntry(
    DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&& other) = default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&
DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::operator=(
    DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&& other) = default;

#if defined(OS_CHROMEOS)
DeviceFactoryMediaToMojoAdapter::DeviceFactoryMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureSystem> capture_system,
    media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner)
    : capture_system_(std::move(capture_system)),
      jpeg_decoder_factory_callback_(std::move(jpeg_decoder_factory_callback)),
      jpeg_decoder_task_runner_(std::move(jpeg_decoder_task_runner)),
      has_called_get_device_infos_(false),
      weak_factory_(this) {}
#else
DeviceFactoryMediaToMojoAdapter::DeviceFactoryMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureSystem> capture_system)
    : capture_system_(std::move(capture_system)),
      has_called_get_device_infos_(false) {}
#endif  // defined(OS_CHROMEOS)

DeviceFactoryMediaToMojoAdapter::~DeviceFactoryMediaToMojoAdapter() = default;

void DeviceFactoryMediaToMojoAdapter::GetDeviceInfos(
    GetDeviceInfosCallback callback) {
  capture_system_->GetDeviceInfosAsync(
      base::BindOnce(&TranslateDeviceInfos, std::move(callback)));
  has_called_get_device_infos_ = true;
}

void DeviceFactoryMediaToMojoAdapter::CreateDevice(
    const std::string& device_id,
    mojo::PendingReceiver<mojom::Device> device_receiver,
    CreateDeviceCallback callback) {
  auto active_device_iter = active_devices_by_id_.find(device_id);
  if (active_device_iter != active_devices_by_id_.end()) {
    // The requested device is already in use.
    // Revoke the access and close the device, then bind to the new receiver.
    ActiveDeviceEntry& device_entry = active_device_iter->second;
    device_entry.receiver->reset();
    device_entry.device->Stop();
    device_entry.receiver->Bind(std::move(device_receiver));
    device_entry.receiver->set_disconnect_handler(base::BindOnce(
        &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
        base::Unretained(this), device_id));
    std::move(callback).Run(mojom::DeviceAccessResultCode::SUCCESS);
    return;
  }

  auto create_and_add_new_device_cb =
      base::BindOnce(&DeviceFactoryMediaToMojoAdapter::CreateAndAddNewDevice,
                     weak_factory_.GetWeakPtr(), device_id,
                     std::move(device_receiver), std::move(callback));

  if (has_called_get_device_infos_) {
    std::move(create_and_add_new_device_cb).Run();
    return;
  }

  capture_system_->GetDeviceInfosAsync(
      base::BindOnce(&DiscardDeviceInfosAndCallContinuation,
                     std::move(create_and_add_new_device_cb)));
  has_called_get_device_infos_ = true;
}

void DeviceFactoryMediaToMojoAdapter::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingRemote<mojom::Producer> producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
        virtual_device_receiver) {
  NOTIMPLEMENTED();
}

void DeviceFactoryMediaToMojoAdapter::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojo::PendingReceiver<mojom::TextureVirtualDevice>
        virtual_device_receiver) {
  NOTIMPLEMENTED();
}

void DeviceFactoryMediaToMojoAdapter::RegisterVirtualDevicesChangedObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
    bool raise_event_if_virtual_devices_already_present) {
  NOTIMPLEMENTED();
}

void DeviceFactoryMediaToMojoAdapter::CreateAndAddNewDevice(
    const std::string& device_id,
    mojo::PendingReceiver<mojom::Device> device_receiver,
    CreateDeviceCallback callback) {
  std::unique_ptr<media::VideoCaptureDevice> media_device =
      capture_system_->CreateDevice(device_id);
  if (media_device == nullptr) {
    std::move(callback).Run(
        mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
    return;
  }

  // Add entry to active_devices to keep track of it
  ActiveDeviceEntry device_entry;

#if defined(OS_CHROMEOS)
  device_entry.device = std::make_unique<DeviceMediaToMojoAdapter>(
      std::move(media_device), jpeg_decoder_factory_callback_,
      jpeg_decoder_task_runner_);
#else
  device_entry.device =
      std::make_unique<DeviceMediaToMojoAdapter>(std::move(media_device));
#endif  // defined(OS_CHROMEOS)
  device_entry.receiver = std::make_unique<mojo::Receiver<mojom::Device>>(
      device_entry.device.get(), std::move(device_receiver));
  device_entry.receiver->set_disconnect_handler(base::BindOnce(
      &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
      base::Unretained(this), device_id));
  active_devices_by_id_[device_id] = std::move(device_entry);

  std::move(callback).Run(mojom::DeviceAccessResultCode::SUCCESS);
}

void DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose(
    const std::string& device_id) {
  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_LOST_CONNECTION_TO_BROWSER);

  active_devices_by_id_[device_id].device->Stop();
  active_devices_by_id_.erase(device_id);
}

}  // namespace video_capture
