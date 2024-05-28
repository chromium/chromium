// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DELEGATE_GPU_HELPER_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DELEGATE_GPU_HELPER_H_

#include <vector>

#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// V4L2CaptureDelegate GPU memory buffer helper.
// This class is a helper to covert video capture data into `NV12` format and
// copy into GPU memory buffer.
class CAPTURE_EXPORT V4L2CaptureDelegateGpuHelper {
 public:
  explicit V4L2CaptureDelegateGpuHelper(
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support = nullptr);

  V4L2CaptureDelegateGpuHelper(const V4L2CaptureDelegateGpuHelper&) = delete;
  V4L2CaptureDelegateGpuHelper& operator=(const V4L2CaptureDelegateGpuHelper&) =
      delete;

  ~V4L2CaptureDelegateGpuHelper();

 public:
  // The `V4L2CaptureDelegate` calls and passes the member
  // `V4L2CaptureDelegate::client_`. This method requests the GPU memory buffer
  // by `V4L2CaptureDelegate::client_`. Then, it converts and copies the capture
  // data into the GPU memory buffer with `NV12` format. At last, it notifies
  // the client that data coming by calling
  // `VideoCaptureDeviceClient::OnIncomingCapturedBufferExt()`.
  // The |rotation| value should be 0, 90, 180, or 270.
  int OnIncomingCapturedData(VideoCaptureDevice::Client* client,
                             const uint8_t* sample,
                             size_t sample_size,
                             const VideoCaptureFormat& format,
                             const gfx::ColorSpace& data_color_space,
                             int rotation,
                             base::TimeTicks reference_time,
                             base::TimeDelta timestamp,
                             int frame_feedback_id = 0);

 private:
  int ConvertCaptureDataToNV12(const uint8_t* sample,
                               size_t sample_size,
                               const VideoCaptureFormat& format,
                               const gfx::Size& dimensions,
                               const gfx::ColorSpace& data_color_space,
                               int rotation,
                               uint8_t* dst_y,
                               uint8_t* dst_uv,
                               int dst_stride_y,
                               int dst_stride_uv);

  // This method directly converts the sample data into `NV12` format when the
  // input `VideoCaptureFormat` supported.
  int FastConvertToNV12(const uint8_t* sample,
                        size_t sample_size,
                        const VideoCaptureFormat& format,
                        uint8_t* dst_y,
                        uint8_t* dst_uv,
                        int dst_stride_y,
                        int dst_stride_uv);

  bool IsNV12ConvertSupported(uint32_t fourcc);

  std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support_;
  // I420 buffer used when can't directly convert video sample data into `NV12`
  // format.
  std::vector<uint8_t> i420_buffer_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DELEGATE_GPU_HELPER_H_
