// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/ash/camera_hal_dispatcher_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

namespace {

gpu::GpuMemoryBufferManager* g_gpu_buffer_manager = nullptr;

}  // namespace

VideoCaptureDeviceFactoryChromeOS::VideoCaptureDeviceFactoryChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_screen_observer)
    : task_runner_for_screen_observer_(task_runner_for_screen_observer),
      camera_hal_ipc_thread_("CameraHalIpcThread"),
      initialized_(Init()) {}

VideoCaptureDeviceFactoryChromeOS::~VideoCaptureDeviceFactoryChromeOS() {
  CameraAppDeviceBridgeImpl::GetInstance()->UnsetCameraInfoGetter();

  auto* camera_app_device_bridge = CameraAppDeviceBridgeImpl::GetInstance();
  camera_app_device_bridge->UnsetCameraInfoGetter();
  camera_app_device_bridge->UnsetVirtualDeviceController();

  camera_hal_delegate_->Reset();
  camera_hal_ipc_thread_.Stop();
}

std::unique_ptr<VideoCaptureDevice>
VideoCaptureDeviceFactoryChromeOS::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    return std::unique_ptr<VideoCaptureDevice>();
  }
  return camera_hal_delegate_->CreateDevice(task_runner_for_screen_observer_,
                                            device_descriptor);
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
  if (!camera_hal_ipc_thread_.Start()) {
    LOG(ERROR) << "Module thread failed to start";
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!CameraHalDispatcherImpl::GetInstance()->IsStarted()) {
    LOG(ERROR) << "CameraHalDispatcherImpl is not started";
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  camera_hal_delegate_ =
      new CameraHalDelegate(camera_hal_ipc_thread_.task_runner());

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
