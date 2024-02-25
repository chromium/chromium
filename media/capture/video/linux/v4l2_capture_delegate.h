// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DELEGATE_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DELEGATE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/capture/video/linux/scoped_v4l2_device_fd.h"
#include "media/capture/video/linux/v4l2_capture_device_impl.h"
#include "media/capture/video/video_capture_device.h"

#if BUILDFLAG(IS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

namespace base {
class Location;
}  // namespace base

namespace media {

#if BUILDFLAG(IS_LINUX)
class V4L2CaptureDelegateGpuHelper;
#endif  // BUILDFLAG(IS_LINUX)

// Class doing the actual Linux capture using V4L2 API. V4L2 SPLANE/MPLANE
// capture specifics are implemented in derived classes. Created on the owner's
// thread, otherwise living, operating and destroyed on |v4l2_task_runner_|.
class CAPTURE_EXPORT V4L2CaptureDelegate final {
 public:
  // Retrieves the #planes for a given |fourcc|, or 0 if unknown.
  static size_t GetNumPlanesForFourCc(uint32_t fourcc);
  // Returns the Chrome pixel format for |v4l2_fourcc| or PIXEL_FORMAT_UNKNOWN.
  static VideoPixelFormat V4l2FourCcToChromiumPixelFormat(
      uint32_t v4l2_fourcc);

  // Composes a list of usable and supported pixel formats, in order of
  // preference, with MJPEG prioritised depending on |prefer_mjpeg|.
  static std::vector<uint32_t> GetListOfUsableFourCcs(bool prefer_mjpeg);

  V4L2CaptureDelegate(
      V4L2CaptureDevice* v4l2,
      const VideoCaptureDeviceDescriptor& device_descriptor,
      const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
      int power_line_frequency,
      int rotation);

  V4L2CaptureDelegate(const V4L2CaptureDelegate&) = delete;
  V4L2CaptureDelegate& operator=(const V4L2CaptureDelegate&) = delete;

  ~V4L2CaptureDelegate();

  // Forward-to versions of VideoCaptureDevice virtual methods.
  void AllocateAndStart(int width,
                        int height,
                        float frame_rate,
                        std::unique_ptr<VideoCaptureDevice::Client> client);
  void StopAndDeAllocate();

  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback);

  void GetPhotoState(VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);

  void SetRotation(int rotation);

  base::WeakPtr<V4L2CaptureDelegate> GetWeakPtr();

  static bool IsBlockedControl(int control_id);
  static bool IsControllableControl(
      int control_id,
      const base::RepeatingCallback<int(int, void*)>& do_ioctl);

  void SetGPUEnvironmentForTesting(
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support);

 private:
  friend class V4L2CaptureDelegateTest;

  class BufferTracker;

  // Running DoIoctl() on some devices, especially shortly after (re)opening the
  // device file descriptor or (re)starting streaming, can fail but works after
  // retrying (https://crbug.com/670262). Returns false if the |request| ioctl
  // fails too many times.
  bool RunIoctl(int request, void* argp);

  // Simple wrapper to do HANDLE_EINTR(v4l2_->ioctl(device_fd_.get(), ...)).
  int DoIoctl(int request, void* argp);

  // Check whether the control is controllable (and not changed automatically).
  bool IsControllableControl(int control_id);

  // Subscribe and unsubscribe control events as needed.
  void ReplaceControlEventSubscriptions();

  // Creates a mojom::RangePtr with the (min, max, current, step) values of the
  // control associated with |control_id|. Returns an empty Range otherwise.
  mojom::RangePtr RetrieveUserControlRange(int control_id);

  // Sets all user control to their default. Some controls are enabled by
  // another flag, usually having the word "auto" in the name, see
  // IsSpecialControl() in the .cc file. These flags are preset beforehand, then
  // set to their defaults individually afterwards.
  void ResetUserAndCameraControlsToDefault();

  // VIDIOC_QUERYBUFs a buffer from V4L2, creates a BufferTracker for it and
  // enqueues it (VIDIOC_QBUF) back into V4L2.
  bool MapAndQueueBuffer(int index);

  bool StartStream();
  void DoCapture();
  bool StopStream();

  void SetErrorState(VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason);

#if BUILDFLAG(IS_LINUX)
  // Systems which describe a "color space" usually map that to one or more of
  // {primary, matrix, transfer, range}. BuildColorSpaceFromv4l2() will use the
  // matched value as first priority. Otherwise, if there is no best matching
  // value, it will be a value with a different name but no essential
  // difference and add a corresponding comments like: "SRGB and BT709 use the
  // same transform".
  gfx::ColorSpace BuildColorSpaceFromv4l2();
#endif

  const raw_ptr<V4L2CaptureDevice> v4l2_;
  const scoped_refptr<base::SingleThreadTaskRunner> v4l2_task_runner_;
  const VideoCaptureDeviceDescriptor device_descriptor_;
  const int power_line_frequency_;

  // The following members are only known on AllocateAndStart().
  VideoCaptureFormat capture_format_;
  v4l2_format video_fmt_;
  std::unique_ptr<VideoCaptureDevice::Client> client_;
  ScopedV4L2DeviceFD device_fd_;

  base::queue<VideoCaptureDevice::TakePhotoCallback> take_photo_callbacks_;

  // Vector of BufferTracker to keep track of mmap()ed pointers and their use.
  std::vector<scoped_refptr<BufferTracker>> buffer_tracker_pool_;

  bool is_capturing_;
  int timeout_count_;

  base::TimeTicks first_ref_time_;

  // Clockwise rotation in degrees. This value should be 0, 90, 180, or 270.
  int rotation_;

#if BUILDFLAG(IS_LINUX)
  // Support GPU memory buffer.
  bool use_gpu_buffer_;
  std::unique_ptr<V4L2CaptureDelegateGpuHelper> v4l2_gpu_helper_;
#endif  // BUILDFLAG(IS_LINUX)
  // For GPU Environment Testing.
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support_test_;

  base::WeakPtrFactory<V4L2CaptureDelegate> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DELEGATE_H_
