// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_FACTORY_CHROMEOS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_FACTORY_CHROMEOS_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace gpu {
class SharedImageInterface;
class GpuChannelHost;
}

namespace media {

using MojoMjpegDecodeAcceleratorFactoryCB = base::RepeatingCallback<void(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>)>;

class CAPTURE_EXPORT VideoCaptureDeviceFactoryChromeOS final
    : public VideoCaptureDeviceFactory {
 public:
  explicit VideoCaptureDeviceFactoryChromeOS(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  VideoCaptureDeviceFactoryChromeOS(const VideoCaptureDeviceFactoryChromeOS&) =
      delete;
  VideoCaptureDeviceFactoryChromeOS& operator=(
      const VideoCaptureDeviceFactoryChromeOS&) = delete;

  ~VideoCaptureDeviceFactoryChromeOS() override;

  // VideoCaptureDeviceFactory interface implementations.
  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) final;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

  static gpu::GpuMemoryBufferManager* GetBufferManager();
  static void SetGpuBufferManager(gpu::GpuMemoryBufferManager* buffer_manager);

  static void SetGpuChannelHost(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);
  static scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost();

  static gpu::SharedImageInterface* GetSharedImageInterface();
  static void SetSharedImageInterface(
      scoped_refptr<gpu::SharedImageInterface> shared_image_interface);

  // This is only for vcd unittests to make sure CameraHalDelegate get the
  // camera module. It should not be invoked in the production code.
  // It will return true immediately when CameraModule is ready for
  // CameraHalDelegate or return false after 10 seconds.
  bool WaitForCameraServiceReadyForTesting();

 private:
  // Initializes the factory. The factory is functional only after this call
  // succeeds.
  bool Init();

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Communication interface to the camera HAL.
  std::unique_ptr<CameraHalDelegate> camera_hal_delegate_;

  // VideoCaptureDeviceChromeosDelegate instances saved in
  // |camera_hal_delegate_| must be freed on the sequence which |CreateDevice()|
  // was called. To keep thread-safe and avoid dangling pointers,
  // |camera_hal_delegate_| has to be freed on |vcd_task_runner_|.
  scoped_refptr<base::SequencedTaskRunner> vcd_task_runner_;

  bool initialized_;

  base::WeakPtrFactory<VideoCaptureDeviceFactoryChromeOS> weak_ptr_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_VIDEO_CAPTURE_DEVICE_FACTORY_CHROMEOS_H_
