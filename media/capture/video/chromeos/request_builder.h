// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_BUILDER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_BUILDER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video_capture_types.h"

namespace media {

class CameraDeviceContext;

// BufferInfo is used to store information about the buffer that is needed when
// building buffers.
struct BufferInfo {
  uint64_t ipc_id;
  gfx::Size dimension;
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
  uint32_t drm_format;
  cros::mojom::HalPixelFormat hal_pixel_format;
  uint64_t modifier;
};

// RequestBuilder is used to build capture request that will be sent to camera
// HAL process.
class CAPTURE_EXPORT RequestBuilder {
 public:
  using RequestBufferCallback =
      base::RepeatingCallback<std::optional<BufferInfo>(StreamType)>;

  RequestBuilder(CameraDeviceContext* device_context,
                 // Callback to request buffer from StreamBufferManager. Having
                 // this callback, we do not need to include StreamBufferManager
                 // when requesting buffer.
                 RequestBufferCallback request_buffer_callback,
                 bool use_buffer_management_apis);
  ~RequestBuilder();

  // Builds a capture request by given streams and settings.
  cros::mojom::Camera3CaptureRequestPtr BuildRequest(
      std::set<StreamType> stream_types,
      cros::mojom::CameraMetadataPtr settings);

  // Used to create Camera3StreamBuffer for Camera3CaptureRequest or
  // Camera3StreamBufferRet.
  cros::mojom::Camera3StreamBufferPtr CreateStreamBuffer(
      StreamType stream_type,
      std::optional<BufferInfo> buffer_info);

 private:
  cros::mojom::CameraBufferHandlePtr CreateCameraBufferHandle(
      StreamType stream_type,
      BufferInfo buffer_info);

  raw_ptr<CameraDeviceContext> device_context_;

  // The frame number. Increased by one for each capture request sent.
  uint32_t frame_number_;

  RequestBufferCallback request_buffer_callback_;

  // Set true if the buffer management APIs are enabled. If true, capture
  // requests do not contain buffer handles.
  bool use_buffer_management_apis_;
};
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_BUILDER_H_
