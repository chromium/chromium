// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of VideoCaptureDevice.
// V4L2 is used for capturing. V4L2 does not provide its own thread for
// capturing so this implementation uses a Chromium thread for fetching frames
// from V4L2.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_

#include <stdint.h>

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/capture/video/linux/v4l2_capture_device_impl.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace base {
class WaitableEvent;
}

namespace media {

class V4L2CaptureDelegate;

// Linux V4L2 implementation of VideoCaptureDevice.
class CAPTURE_EXPORT VideoCaptureDeviceLinux : public VideoCaptureDevice {
 public:
  static VideoPixelFormat V4l2FourCcToChromiumPixelFormat(uint32_t v4l2_fourcc);
  static std::vector<uint32_t> GetListOfUsableFourCCs(bool favour_mjpeg);

  VideoCaptureDeviceLinux() = delete;

  explicit VideoCaptureDeviceLinux(
      scoped_refptr<V4L2CaptureDevice> v4l2,
      const VideoCaptureDeviceDescriptor& device_descriptor);

  VideoCaptureDeviceLinux(const VideoCaptureDeviceLinux&) = delete;
  VideoCaptureDeviceLinux& operator=(const VideoCaptureDeviceLinux&) = delete;

  ~VideoCaptureDeviceLinux() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

  void SetGPUEnvironmentForTesting(
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support);

 protected:
  virtual void SetRotation(int rotation);

  const VideoCaptureDeviceDescriptor device_descriptor_;

 private:
  void StopAndDeAllocateInternal(base::WaitableEvent* waiter);

  const scoped_refptr<V4L2CaptureDevice> v4l2_;

  // Internal delegate doing the actual capture setting, buffer allocation and
  // circulation with the V4L2 API. Created in the thread where
  // VideoCaptureDeviceLinux lives but otherwise operating and deleted on
  // |v4l2_thread_|.
  std::unique_ptr<V4L2CaptureDelegate> capture_impl_;

  // For GPU Environment Testing.
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support_test_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // SetRotation() may get called even when the device is not started. When that
  // is the case we remember the value here and use it as soon as the device
  // gets started.
  int rotation_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
