// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/request_builder.h"

#include <utility>

#include "media/capture/video/chromeos/camera_device_context.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

RequestBuilder::RequestBuilder(CameraDeviceContext* device_context,
                               RequestBufferCallback request_buffer_callback,
                               bool use_buffer_management_apis)
    : device_context_(device_context),
      frame_number_(0),
      request_buffer_callback_(std::move(request_buffer_callback)),
      use_buffer_management_apis_(use_buffer_management_apis) {}

RequestBuilder::~RequestBuilder() = default;

cros::mojom::Camera3CaptureRequestPtr RequestBuilder::BuildRequest(
    std::set<StreamType> stream_types,
    cros::mojom::CameraMetadataPtr settings) {
  auto capture_request = cros::mojom::Camera3CaptureRequest::New();
  for (StreamType stream_type : stream_types) {
    // If the buffer management APIs are enabled, capture requests do not
    // contain buffer handles.
    if (use_buffer_management_apis_) {
      capture_request->output_buffers.push_back(
          CreateStreamBuffer(stream_type, std::nullopt));
      continue;
    }

    std::optional<BufferInfo> buffer_info =
        request_buffer_callback_.Run(stream_type);
    if (!buffer_info) {
      return capture_request;
    }
    capture_request->output_buffers.push_back(
        CreateStreamBuffer(stream_type, std::move(*buffer_info)));
  }
  capture_request->settings = std::move(settings);
  capture_request->frame_number = frame_number_++;

  return capture_request;
}

cros::mojom::Camera3StreamBufferPtr RequestBuilder::CreateStreamBuffer(
    StreamType stream_type,
    std::optional<BufferInfo> buffer_info) {
  cros::mojom::Camera3StreamBufferPtr buffer =
      cros::mojom::Camera3StreamBuffer::New();
  buffer->stream_id = static_cast<uint64_t>(stream_type);
  buffer->status = cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK;
  if (buffer_info.has_value()) {
    buffer->buffer_id = buffer_info->ipc_id;
    buffer->buffer_handle =
        CreateCameraBufferHandle(stream_type, std::move(*buffer_info));
  } else {
    buffer->buffer_id = cros::mojom::NO_BUFFER_BUFFER_ID;
    buffer->buffer_handle = nullptr;
  }
  return buffer;
}

cros::mojom::CameraBufferHandlePtr RequestBuilder::CreateCameraBufferHandle(
    StreamType stream_type,
    BufferInfo buffer_info) {
  auto buffer_handle = cros::mojom::CameraBufferHandle::New();

  buffer_handle->buffer_id = buffer_info.ipc_id;
  buffer_handle->drm_format = buffer_info.drm_format;
  buffer_handle->hal_pixel_format = buffer_info.hal_pixel_format;
  buffer_handle->has_modifier = true;
  buffer_handle->modifier = buffer_info.modifier;
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

}  // namespace media
