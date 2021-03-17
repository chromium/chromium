// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

namespace media {

namespace {

void InvalidateDevicePtrsOnDeviceIpcThread(
    base::WeakPtr<CameraAppDeviceImpl> device,
    base::OnceClosure callback) {
  if (device) {
    device->InvalidatePtrs(std::move(callback));
  }
}

}  // namespace

CameraAppDeviceBridgeImpl::CameraAppDeviceBridgeImpl() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool use_fake_camera =
      command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream);
  bool use_file_camera =
      command_line->HasSwitch(switches::kUseFileForFakeVideoCapture);
  is_supported_ =
      ShouldUseCrosCameraService() && !use_fake_camera && !use_file_camera;
}

CameraAppDeviceBridgeImpl::~CameraAppDeviceBridgeImpl() = default;

// static
CameraAppDeviceBridgeImpl* CameraAppDeviceBridgeImpl::GetInstance() {
  return base::Singleton<CameraAppDeviceBridgeImpl>::get();
}

void CameraAppDeviceBridgeImpl::BindReceiver(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CameraAppDeviceBridgeImpl::OnVideoCaptureDeviceCreated(
    const std::string& device_id,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner) {
  base::AutoLock lock(task_runner_map_lock_);
  DCHECK_EQ(ipc_task_runners_.count(device_id), 0u);
  ipc_task_runners_.emplace(device_id, ipc_task_runner);
}

void CameraAppDeviceBridgeImpl::OnVideoCaptureDeviceClosing(
    const std::string& device_id) {
  base::AutoLock lock(task_runner_map_lock_);
  DCHECK_EQ(ipc_task_runners_.count(device_id), 1u);
  ipc_task_runners_[device_id]->PostTask(
      FROM_HERE, base::BindOnce(&InvalidateDevicePtrsOnDeviceIpcThread,
                                GetWeakCameraAppDevice(device_id),
                                base::DoNothing::Once()));
  ipc_task_runners_.erase(device_id);
}

void CameraAppDeviceBridgeImpl::OnDeviceMojoDisconnected(
    const std::string& device_id) {
  auto remove_device = media::BindToCurrentLoop(
      base::BindOnce(&CameraAppDeviceBridgeImpl::RemoveCameraAppDevice,
                     base::Unretained(this), device_id));
  {
    base::AutoLock lock(task_runner_map_lock_);
    auto it = ipc_task_runners_.find(device_id);
    if (it != ipc_task_runners_.end()) {
      it->second->PostTask(
          FROM_HERE, base::BindOnce(&InvalidateDevicePtrsOnDeviceIpcThread,
                                    GetWeakCameraAppDevice(device_id),
                                    std::move(remove_device)));
      return;
    }
  }
  std::move(remove_device).Run();
}

void CameraAppDeviceBridgeImpl::SetCameraInfoGetter(
    CameraInfoGetter camera_info_getter) {
  base::AutoLock lock(camera_info_getter_lock_);
  camera_info_getter_ = std::move(camera_info_getter);
}

void CameraAppDeviceBridgeImpl::UnsetCameraInfoGetter() {
  base::AutoLock lock(camera_info_getter_lock_);
  camera_info_getter_ = {};
}

void CameraAppDeviceBridgeImpl::SetVirtualDeviceController(
    VirtualDeviceController virtual_device_controller) {
  base::AutoLock lock(virtual_device_controller_lock_);
  virtual_device_controller_ = std::move(virtual_device_controller);
}

void CameraAppDeviceBridgeImpl::UnsetVirtualDeviceController() {
  base::AutoLock lock(virtual_device_controller_lock_);
  virtual_device_controller_ = {};
}

base::WeakPtr<CameraAppDeviceImpl>
CameraAppDeviceBridgeImpl::GetWeakCameraAppDevice(
    const std::string& device_id) {
  base::AutoLock lock(device_map_lock_);
  auto it = camera_app_devices_.find(device_id);
  if (it == camera_app_devices_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

void CameraAppDeviceBridgeImpl::RemoveCameraAppDevice(
    const std::string& device_id) {
  base::AutoLock lock(device_map_lock_);
  auto it = camera_app_devices_.find(device_id);
  if (it == camera_app_devices_.end()) {
    return;
  }
  camera_app_devices_.erase(it);
}

void CameraAppDeviceBridgeImpl::GetCameraAppDevice(
    const std::string& device_id,
    GetCameraAppDeviceCallback callback) {
  DCHECK(is_supported_);

  mojo::PendingRemote<cros::mojom::CameraAppDevice> device_remote;
  auto* device = GetOrCreateCameraAppDevice(device_id);
  DCHECK(device);

  device->BindReceiver(device_remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(cros::mojom::GetCameraAppDeviceStatus::SUCCESS,
                          std::move(device_remote));
}

media::CameraAppDeviceImpl*
CameraAppDeviceBridgeImpl::GetOrCreateCameraAppDevice(
    const std::string& device_id) {
  base::AutoLock lock(device_map_lock_);
  auto it = camera_app_devices_.find(device_id);
  if (it != camera_app_devices_.end()) {
    return it->second.get();
  }

  base::AutoLock camera_info_lock(camera_info_getter_lock_);
  // Since we ensure that VideoCaptureDeviceFactory is created before binding
  // CameraAppDeviceBridge and VideoCaptureDeviceFactory is only destroyed when
  // the video capture service dies, we can guarantee that |camera_info_getter_|
  // is always valid here.
  DCHECK(camera_info_getter_);

  auto device_info = camera_info_getter_.Run(device_id);
  auto device_impl = std::make_unique<media::CameraAppDeviceImpl>(
      device_id, std::move(device_info));
  auto result = camera_app_devices_.emplace(device_id, std::move(device_impl));
  return result.first->second.get();
}

void CameraAppDeviceBridgeImpl::IsSupported(IsSupportedCallback callback) {
  std::move(callback).Run(is_supported_);
}

void CameraAppDeviceBridgeImpl::SetMultipleStreamsEnabled(
    const std::string& device_id,
    bool enabled,
    SetMultipleStreamsEnabledCallback callback) {
  base::AutoLock lock(virtual_device_controller_lock_);
  if (!virtual_device_controller_) {
    std::move(callback).Run(false);
    return;
  }

  virtual_device_controller_.Run(device_id, enabled);
  std::move(callback).Run(true);
}

}  // namespace media
