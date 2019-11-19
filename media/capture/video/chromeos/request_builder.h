// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_BUILDER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_BUILDER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/optional.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/binding.h"

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
};

// RequestBuilder is used to build capture request that will be sent to camera
// HAL process.
class CAPTURE_EXPORT RequestBuilder {
 public:
  using RequestBufferCallback = base::RepeatingCallback<
      base::Optional<BufferInfo>(StreamType, base::Optional<uint64_t>)>;

  RequestBuilder(CameraDeviceContext* device_context,
                 // Callback to request buffer from StreamBufferManager. Having
                 // this callback, we do not need to include StreamBufferManager
                 // when requesting buffer.
                 RequestBufferCallback request_buffer_callback);
  ~RequestBuilder();

  // Builds a capture request by given streams and settings. The
  // |input_buffer_id| is used for reprocess request.
  cros::mojom::Camera3CaptureRequestPtr BuildRequest(
      std::set<StreamType> stream_types,
      cros::mojom::CameraMetadataPtr settings,
      base::Optional<uint64_t> input_buffer_id);

 private:
  cros::mojom::CameraBufferHandlePtr CreateCameraBufferHandle(
      StreamType stream_type,
      BufferInfo buffer_info);

  cros::mojom::Camera3StreamBufferPtr CreateStreamBuffer(
      StreamType stream_type,
      uint64_t buffer_id,
      cros::mojom::CameraBufferHandlePtr buffer_handle);

  CameraDeviceContext* device_context_;

  // The frame number. Increased by one for each capture request sent.
  uint32_t frame_number_;

  RequestBufferCallback request_buffer_callback_;
};
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_REQUEST_BUILDER_H_
