// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace media {

namespace {

gpu::GpuMemoryBufferManager* g_gpu_buffer_manager = nullptr;
scoped_refptr<gpu::SharedImageInterface> g_shared_image_interface = nullptr;
scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_ = nullptr;

}  // namespace

VideoCaptureDeviceFactoryChromeOS::VideoCaptureDeviceFactoryChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner), initialized_(Init()) {}

VideoCaptureDeviceFactoryChromeOS::~VideoCaptureDeviceFactoryChromeOS() {
  CameraAppDeviceBridgeImpl::GetInstance()->UnsetCameraInfoGetter();

  auto* camera_app_device_bridge = CameraAppDeviceBridgeImpl::GetInstance();
  camera_app_device_bridge->UnsetCameraInfoGetter();
  camera_app_device_bridge->UnsetVirtualDeviceController();
  if (camera_hal_delegate_) {
    if (vcd_task_runner_ && !vcd_task_runner_->RunsTasksInCurrentSequence()) {
      vcd_task_runner_->DeleteSoon(FROM_HERE, std::move(camera_hal_delegate_));
    }
  }
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryChromeOS::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::
            kCrosHalV3DeviceDelegateFailedToInitializeCameraDevice);
  }
  auto device =
      camera_hal_delegate_->CreateDevice(ui_task_runner_, device_descriptor);

  if (!device) {
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::
            kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed);
  }
  if (!vcd_task_runner_) {
    vcd_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return VideoCaptureErrorOrDevice(std::move(device));
}

void VideoCaptureDeviceFactoryChromeOS::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    std::move(callback).Run({});
    return;
  }

  camera_hal_delegate_->GetDevicesInfo(std::move(callback));
}

// static
gpu::GpuMemoryBufferManager*
VideoCaptureDeviceFactoryChromeOS::GetBufferManager() {
  return g_gpu_buffer_manager;
}

// static
void VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
    gpu::GpuMemoryBufferManager* buffer_manager) {
  g_gpu_buffer_manager = buffer_manager;
}

// static
void VideoCaptureDeviceFactoryChromeOS::SetGpuChannelHost(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  gpu_channel_host_ = std::move(gpu_channel_host);
}

// static
scoped_refptr<gpu::GpuChannelHost>
VideoCaptureDeviceFactoryChromeOS::GetGpuChannelHost() {
  return gpu_channel_host_;
}

// static
gpu::SharedImageInterface*
VideoCaptureDeviceFactoryChromeOS::GetSharedImageInterface() {
  return g_shared_image_interface.get();
}

// static
void VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface) {
  // If both SharedImageInterface have a valid pointer, then making sure they
  // are same in order to catch any issues caused from setting it to different
  // values multiple times in a given process.
  if (shared_image_interface && g_shared_image_interface) {
    CHECK_EQ(shared_image_interface.get(), g_shared_image_interface.get());
    return;
  }
  g_shared_image_interface = std::move(shared_image_interface);
}

bool VideoCaptureDeviceFactoryChromeOS::Init() {
  camera_hal_delegate_ = std::make_unique<CameraHalDelegate>(ui_task_runner_);

  if (!camera_hal_delegate_->Init()) {
    LOG(ERROR) << "Failed to initialize CameraHalDelegate";
    camera_hal_delegate_.reset();
    return false;
  }

  camera_hal_delegate_->BootStrapCameraServiceConnection();

  // Since we will unset camera info getter and virtual device controller before
  // invalidate |camera_hal_delegate_| in the destructor, it should be safe to
  // use base::Unretained() here.
  auto* camera_app_device_bridge = CameraAppDeviceBridgeImpl::GetInstance();
  camera_app_device_bridge->SetCameraInfoGetter(
      base::BindRepeating(&CameraHalDelegate::GetCameraInfoFromDeviceId,
                          base::Unretained(camera_hal_delegate_.get())));
  camera_app_device_bridge->SetVirtualDeviceController(
      base::BindRepeating(&CameraHalDelegate::EnableVirtualDevice,
                          base::Unretained(camera_hal_delegate_.get())));
  return true;
}

bool VideoCaptureDeviceFactoryChromeOS::WaitForCameraServiceReadyForTesting() {
  if (!camera_hal_delegate_) {
    return false;
  }
  return camera_hal_delegate_->WaitForCameraModuleReadyForTesting();  // IN-TEST
}

}  // namespace media
