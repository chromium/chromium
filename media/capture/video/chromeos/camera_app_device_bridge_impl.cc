// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "media/base/media_switches.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

namespace media {

CameraAppDeviceBridgeImpl::CameraAppDeviceBridgeImpl()
    : ipc_thread_("CameraAppDeviceBridgeThread") {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool use_fake_camera =
      command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream);
  bool use_file_camera =
      command_line->HasSwitch(switches::kUseFileForFakeVideoCapture);
  is_supported_ =
      ShouldUseCrosCameraService() && !use_fake_camera && !use_file_camera;
  receivers_.set_disconnect_with_reason_handler(
      base::BindRepeating([](uint32_t reason, const std::string& description) {
        CAMERA_LOG(EVENT) << "Receiver disconnected, reason " << reason << " ("
                          << description << ")";
      }));
  CHECK(ipc_thread_.Start())
      << "Can't bootstrap the CameraAppDeviceBridge thread.";
  ipc_task_runner_ = ipc_thread_.task_runner();
}

CameraAppDeviceBridgeImpl::~CameraAppDeviceBridgeImpl() {
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CameraAppDeviceBridgeImpl::StopOnIPCThread,
                                base::Unretained(this)));
  ipc_thread_.Stop();
}

// static
CameraAppDeviceBridgeImpl* CameraAppDeviceBridgeImpl::GetInstance() {
  return base::Singleton<CameraAppDeviceBridgeImpl>::get();
}

void CameraAppDeviceBridgeImpl::BindReceiver(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver) {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraAppDeviceBridgeImpl::BindReceiverOnIPCThread,
                     base::Unretained(this), std::move(receiver)));
}

void CameraAppDeviceBridgeImpl::BindReceiverOnIPCThread(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver) {
  CHECK(ipc_task_runner_->BelongsToCurrentThread());
  receivers_.Add(this, std::move(receiver));
}

void CameraAppDeviceBridgeImpl::OnVideoCaptureDeviceCreated(
    const std::string& device_id,
    scoped_refptr<base::SingleThreadTaskRunner> vcd_task_runner) {
  {
    base::AutoLock lock(task_runner_map_lock_);
    DCHECK_EQ(vcd_task_runners_.count(device_id), 0u);
    vcd_task_runners_.emplace(device_id, vcd_task_runner);
  }

  // Update the cached camera info while VCD is connected as well so that when
  // the camera service is restarted the camera info can be updated properly.
  UpdateCameraInfo(device_id);
}

void CameraAppDeviceBridgeImpl::OnVideoCaptureDeviceClosing(
    const std::string& device_id) {
  auto remove_vcd_task_runner =
      base::BindOnce(&CameraAppDeviceBridgeImpl::RemoveVCDTaskRunner,
                     base::Unretained(this), device_id);
  base::AutoLock lock(task_runner_map_lock_);
  DCHECK_EQ(vcd_task_runners_.count(device_id), 1u);

  // Since the IPC thread is owned by VCD and the CameraAppBridgeImpl is a
  // singleton which has longer lifetime than VCD, it is safe to use
  // base::Unretained(this) here.
  vcd_task_runners_[device_id]->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CameraAppDeviceBridgeImpl::InvalidateDevicePtrsOnDeviceIpcThread,
          base::Unretained(this), device_id,
          /* should_disable_new_ptrs */ false,
          std::move(remove_vcd_task_runner)));
}

void CameraAppDeviceBridgeImpl::OnDeviceMojoDisconnected(
    const std::string& device_id) {
  auto remove_device = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&CameraAppDeviceBridgeImpl::RemoveCameraAppDevice,
                     base::Unretained(this), device_id));
  {
    base::AutoLock lock(task_runner_map_lock_);
    auto it = vcd_task_runners_.find(device_id);
    if (it != vcd_task_runners_.end()) {
      // Since the IPC thread is owned by VCD and the CameraAppBridgeImpl is a
      // singleton which has longer lifetime than VCD, it is safe to use
      // base::Unretained(this) here.
      it->second->PostTask(
          FROM_HERE,
          base::BindOnce(
              &CameraAppDeviceBridgeImpl::InvalidateDevicePtrsOnDeviceIpcThread,
              base::Unretained(this), device_id,
              /* should_disable_new_ptrs */ true, std::move(remove_device)));
      return;
    }
  }
  std::move(remove_device).Run();
}

void CameraAppDeviceBridgeImpl::UpdateCameraInfo(const std::string& device_id) {
  cros::mojom::CameraInfoPtr camera_info;
  {
    base::AutoLock lock(camera_info_getter_lock_);
    DCHECK(camera_info_getter_);
    camera_info = camera_info_getter_.Run(device_id);
  }

  {
    base::AutoLock lock(device_map_lock_);
    auto it = camera_app_devices_.find(device_id);
    if (it != camera_app_devices_.end()) {
      const auto& device = it->second;
      device->OnCameraInfoUpdated(std::move(camera_info));
    }
  }
}

void CameraAppDeviceBridgeImpl::InvalidateDevicePtrsOnDeviceIpcThread(
    const std::string& device_id,
    bool should_disable_new_ptrs,
    base::OnceClosure callback) {
  auto device = GetWeakCameraAppDevice(device_id);
  if (device) {
    device->ResetOnDeviceIpcThread(std::move(callback),
                                   should_disable_new_ptrs);
  } else {
    std::move(callback).Run();
  }
}

void CameraAppDeviceBridgeImpl::SetCameraInfoGetter(
    CameraInfoGetter camera_info_getter) {
  base::AutoLock lock(camera_info_getter_lock_);
  camera_info_getter_ = std::move(camera_info_getter);
}

void CameraAppDeviceBridgeImpl::UnsetCameraInfoGetter() {
  base::AutoLock lock(camera_info_getter_lock_);
  camera_info_getter_ = base::NullCallback();
}

void CameraAppDeviceBridgeImpl::SetVirtualDeviceController(
    VirtualDeviceController virtual_device_controller) {
  base::AutoLock lock(virtual_device_controller_lock_);
  virtual_device_controller_ = std::move(virtual_device_controller);
}

void CameraAppDeviceBridgeImpl::UnsetVirtualDeviceController() {
  base::AutoLock lock(virtual_device_controller_lock_);
  virtual_device_controller_ = base::NullCallback();
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

void CameraAppDeviceBridgeImpl::RemoveVCDTaskRunner(
    const std::string& device_id) {
  base::AutoLock lock(task_runner_map_lock_);
  vcd_task_runners_.erase(device_id);
}

void CameraAppDeviceBridgeImpl::GetCameraAppDevice(
    const std::string& device_id,
    GetCameraAppDeviceCallback callback) {
  CHECK(is_supported_);
  CHECK(ipc_task_runner_->BelongsToCurrentThread());
  CHECK(ui_task_runner_);

  mojo::PendingRemote<cros::mojom::CameraAppDevice> device_remote;
  {
    base::AutoLock lock(device_map_lock_);

    CameraAppDeviceImpl* device;
    auto it = camera_app_devices_.find(device_id);
    if (it != camera_app_devices_.end()) {
      device = it->second.get();
    } else {
      auto device_impl = std::make_unique<media::CameraAppDeviceImpl>(
          device_id, ui_task_runner_);
      const auto& iterator =
          camera_app_devices_.emplace(device_id, std::move(device_impl)).first;
      device = iterator->second.get();
    }
    device->BindReceiver(device_remote.InitWithNewPipeAndPassReceiver());
  }
  UpdateCameraInfo(device_id);
  std::move(callback).Run(cros::mojom::GetCameraAppDeviceStatus::kSuccess,
                          std::move(device_remote));
}

void CameraAppDeviceBridgeImpl::IsSupported(IsSupportedCallback callback) {
  CHECK(ipc_task_runner_->BelongsToCurrentThread());
  std::move(callback).Run(is_supported_);
}

void CameraAppDeviceBridgeImpl::SetVirtualDeviceEnabled(
    const std::string& device_id,
    bool enabled,
    SetVirtualDeviceEnabledCallback callback) {
  CHECK(ipc_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(virtual_device_controller_lock_);
  if (!virtual_device_controller_) {
    std::move(callback).Run(false);
    return;
  }

  virtual_device_controller_.Run(device_id, enabled);
  std::move(callback).Run(true);
}

void CameraAppDeviceBridgeImpl::SetDeviceInUse(const std::string& device_id,
                                               bool in_use) {
  base::AutoLock lock(devices_in_use_lock_);
  if (in_use) {
    devices_in_use_.insert(device_id);
  } else {
    devices_in_use_.erase(device_id);
  }
}

void CameraAppDeviceBridgeImpl::IsDeviceInUse(const std::string& device_id,
                                              IsDeviceInUseCallback callback) {
  bool in_use;
  {
    base::AutoLock lock(devices_in_use_lock_);
    in_use = devices_in_use_.contains(device_id);
  }
  std::move(callback).Run(in_use);
}

void CameraAppDeviceBridgeImpl::StopOnIPCThread() {
  CHECK(ipc_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(device_map_lock_);
  camera_app_devices_.clear();
  receivers_.Clear();
}

void CameraAppDeviceBridgeImpl::SetUITaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
}

}  // namespace media
