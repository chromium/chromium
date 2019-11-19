// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/request_builder.h"

#include <utility>

#include "media/capture/video/chromeos/camera_device_context.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

RequestBuilder::RequestBuilder(CameraDeviceContext* device_context,
                               RequestBufferCallback request_buffer_callback)
    : device_context_(device_context),
      frame_number_(0),
      request_buffer_callback_(std::move(request_buffer_callback)) {}

RequestBuilder::~RequestBuilder() = default;

cros::mojom::Camera3CaptureRequestPtr RequestBuilder::BuildRequest(
    std::set<StreamType> stream_types,
    cros::mojom::CameraMetadataPtr settings,
    base::Optional<uint64_t> input_buffer_id) {
  auto capture_request = cros::mojom::Camera3CaptureRequest::New();
  for (StreamType stream_type : stream_types) {
    base::Optional<BufferInfo> buffer_info;
    if (IsInputStream(stream_type)) {
      DCHECK(input_buffer_id.has_value());
      buffer_info = request_buffer_callback_.Run(stream_type, input_buffer_id);
    } else {
      buffer_info = request_buffer_callback_.Run(stream_type, {});
    }
    if (!buffer_info) {
      return capture_request;
    }
    const uint64_t buffer_ipc_id = buffer_info->ipc_id;
    auto buffer_handle =
        CreateCameraBufferHandle(stream_type, std::move(*buffer_info));
    auto stream_buffer = CreateStreamBuffer(stream_type, buffer_ipc_id,
                                            std::move(buffer_handle));
    if (IsInputStream(stream_type)) {
      capture_request->input_buffer = std::move(stream_buffer);
    } else {
      capture_request->output_buffers.push_back(std::move(stream_buffer));
    }
  }
  capture_request->settings = std::move(settings);
  capture_request->frame_number = frame_number_++;

  return capture_request;
}

cros::mojom::CameraBufferHandlePtr RequestBuilder::CreateCameraBufferHandle(
    StreamType stream_type,
    BufferInfo buffer_info) {
  auto buffer_handle = cros::mojom::CameraBufferHandle::New();

  buffer_handle->buffer_id = buffer_info.ipc_id;
  buffer_handle->drm_format = buffer_info.drm_format;
  buffer_handle->hal_pixel_format = buffer_info.hal_pixel_format;
  buffer_handle->width = buffer_info.dimension.width();
  buffer_handle->height = buffer_info.dimension.height();

  gfx::NativePixmapHandle& native_pixmap_handle =
      buffer_info.gpu_memory_buffer_handle.native_pixmap_handle;

  size_t num_planes = native_pixmap_handle.planes.size();
  std::vector<StreamCaptureInterface::Plane> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    mojo::ScopedHandle mojo_fd = mojo::WrapPlatformHandle(
        mojo::PlatformHandle(std::move(native_pixmap_handle.planes[i].fd)));
    if (!mojo_fd.is_valid()) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle,
          FROM_HERE, "Failed to wrap gpu memory handle");
      return nullptr;
    }
    buffer_handle->fds.push_back(std::move(mojo_fd));
    buffer_handle->strides.push_back(native_pixmap_handle.planes[i].stride);
    buffer_handle->offsets.push_back(native_pixmap_handle.planes[i].offset);
  }

  return buffer_handle;
}

cros::mojom::Camera3StreamBufferPtr RequestBuilder::CreateStreamBuffer(
    StreamType stream_type,
    uint64_t buffer_ipc_id,
    cros::mojom::CameraBufferHandlePtr buffer_handle) {
  cros::mojom::Camera3StreamBufferPtr buffer =
      cros::mojom::Camera3StreamBuffer::New();
  buffer->stream_id = static_cast<uint64_t>(stream_type);
  buffer->buffer_id = buffer_ipc_id;
  buffer->status = cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK;
  buffer->buffer_handle = std::move(buffer_handle);
  return buffer;
}

}  // namespace media
