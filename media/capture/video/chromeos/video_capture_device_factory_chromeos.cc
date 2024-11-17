// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace media {

namespace {

// This class is designed as a singleton because it holds resources that needs
// to be accessed globally. This ensures consistent state and minimizes memory
// usage by sharing resources across components.
class GpuResources : public gpu::GpuMemoryBufferManagerObserver {
 public:
  GpuResources() = default;

  void OnGpuMemoryBufferManagerDestroyed() override {
    base::AutoLock lock(lock_);
    // Invalidate the pointer to avoid dangling reference.
    gpu_buffer_manager_ = nullptr;
  }

  gpu::GpuMemoryBufferManager* GetBufferManager() const {
    base::AutoLock lock(lock_);
    return gpu_buffer_manager_;
  }

  void SetBufferManager(gpu::GpuMemoryBufferManager* buffer_manager) {
    base::AutoLock lock(lock_);
    gpu_buffer_manager_ = buffer_manager;
    if (buffer_manager) {
      buffer_manager->AddObserver(this);
    }
  }

  scoped_refptr<gpu::SharedImageInterface> GetSharedImageInterface() const {
    base::AutoLock lock(lock_);
    return shared_image_interface_;
  }

  void SetSharedImageInterface(
      scoped_refptr<gpu::SharedImageInterface> interface) {
    base::AutoLock lock(lock_);
    //  Ensure only one instance is set
    if (interface && shared_image_interface_) {
      CHECK_EQ(interface, shared_image_interface_);
      return;
    }
    shared_image_interface_ = std::move(interface);
  }

  scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() const {
    base::AutoLock lock(lock_);
    return gpu_channel_host_;
  }

  void SetGpuChannelHost(scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
    base::AutoLock lock(lock_);
    gpu_channel_host_ = std::move(gpu_channel_host);
  }

 private:
  mutable base::Lock lock_;
  raw_ptr<gpu::GpuMemoryBufferManager> gpu_buffer_manager_ GUARDED_BY(lock_) =
      nullptr;
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_
      GUARDED_BY(lock_);
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_ GUARDED_BY(lock_);
};

// Singleton accessor for GpuResources.
static GpuResources& GetGpuResources() {
  static base::NoDestructor<GpuResources> instance;
  return *instance;
}

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
  return GetGpuResources().GetBufferManager();
}

// static
void VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
    gpu::GpuMemoryBufferManager* buffer_manager) {
  GetGpuResources().SetBufferManager(buffer_manager);
}

// static
void VideoCaptureDeviceFactoryChromeOS::SetGpuChannelHost(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  GetGpuResources().SetGpuChannelHost(std::move(gpu_channel_host));
}

// static
scoped_refptr<gpu::GpuChannelHost>
VideoCaptureDeviceFactoryChromeOS::GetGpuChannelHost() {
  return GetGpuResources().GetGpuChannelHost();
}

// static
scoped_refptr<gpu::SharedImageInterface>
VideoCaptureDeviceFactoryChromeOS::GetSharedImageInterface() {
  return GetGpuResources().GetSharedImageInterface().get();
}

// static
void VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface) {
  GetGpuResources().SetSharedImageInterface(std::move(shared_image_interface));
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
