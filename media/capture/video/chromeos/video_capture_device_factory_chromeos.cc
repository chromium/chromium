// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace media {

namespace {

gpu::GpuMemoryBufferManager* g_gpu_buffer_manager = nullptr;

}  // namespace

VideoCaptureDeviceFactoryChromeOS::VideoCaptureDeviceFactoryChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner), initialized_(Init()) {}

VideoCaptureDeviceFactoryChromeOS::~VideoCaptureDeviceFactoryChromeOS() {
  CameraAppDeviceBridgeImpl::GetInstance()->UnsetCameraInfoGetter();

  auto* camera_app_device_bridge = CameraAppDeviceBridgeImpl::GetInstance();
  camera_app_device_bridge->UnsetCameraInfoGetter();
  camera_app_device_bridge->UnsetVirtualDeviceController();

  camera_hal_delegate_->Reset();
  camera_hal_delegate_.reset();
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
  return device ? VideoCaptureErrorOrDevice(std::move(device))
                : VideoCaptureErrorOrDevice(
                      VideoCaptureError::
                          kVideoCaptureDeviceFactoryChromeOSCreateDeviceFailed);
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

bool VideoCaptureDeviceFactoryChromeOS::Init() {
  if (!CameraHalDispatcherImpl::GetInstance()->IsStarted()) {
    LOG(ERROR) << "CameraHalDispatcherImpl is not started";
    return false;
  }

  camera_hal_delegate_ = std::make_unique<CameraHalDelegate>(ui_task_runner_);

  if (!camera_hal_delegate_->Init()) {
    LOG(ERROR) << "Failed to initialize CameraHalDelegate";
    camera_hal_delegate_.reset();
    return false;
  }

  if (!camera_hal_delegate_->RegisterCameraClient()) {
    LOG(ERROR) << "Failed to register camera client";
    return false;
  }

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

}  // namespace media
