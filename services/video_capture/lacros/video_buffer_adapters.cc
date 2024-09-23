// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/video_buffer_adapters.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mojom/buffer_types.mojom.h"
#include "ui/gfx/mojom/native_handle_types.mojom.h"

namespace video_capture {

namespace {

gfx::GpuMemoryBufferHandle ToGfxGpuMemoryBufferHandle(
    crosapi::mojom::GpuMemoryBufferHandlePtr buffer_handle) {
  gfx::GpuMemoryBufferHandle gfx_buffer_handle;
  gfx_buffer_handle.id = gfx::GpuMemoryBufferId(buffer_handle->id);
  gfx_buffer_handle.offset = buffer_handle->offset;
  gfx_buffer_handle.stride = buffer_handle->stride;

  if (buffer_handle->platform_handle) {
    auto& platform_handle = buffer_handle->platform_handle;
    if (platform_handle->is_shared_memory_handle()) {
      gfx_buffer_handle.type = gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
      gfx_buffer_handle.region =
          std::move(platform_handle->get_shared_memory_handle());
    } else if (platform_handle->is_native_pixmap_handle()) {
      gfx_buffer_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
      auto& native_pixmap_handle = platform_handle->get_native_pixmap_handle();
      gfx::NativePixmapHandle gfx_native_pixmap_handle;
      gfx_native_pixmap_handle.planes = std::move(native_pixmap_handle->planes);
      gfx_native_pixmap_handle.modifier = native_pixmap_handle->modifier;
      gfx_buffer_handle.native_pixmap_handle =
          std::move(gfx_native_pixmap_handle);
    } else {
      NOTREACHED_IN_MIGRATION() << "Unexpected new buffer type";
    }
  }
  return gfx_buffer_handle;
}

void OnFrameDone(mojo::Remote<crosapi::mojom::ScopedAccessPermission>
                     remote_access_permission) {
  // There's nothing to do here, except to serve as a keep-alive for the
  // remote until the callback is run. So just let it be destroyed now.
}

std::unique_ptr<media::ScopedFrameDoneHelper> GetAccessPermissionHelper(
    mojo::PendingRemote<crosapi::mojom::ScopedAccessPermission>
        pending_remote_access_permission) {
  return std::make_unique<media::ScopedFrameDoneHelper>(base::BindOnce(
      &OnFrameDone, mojo::Remote<crosapi::mojom::ScopedAccessPermission>(
                        std::move(pending_remote_access_permission))));
}

}  // namespace

media::mojom::VideoBufferHandlePtr ConvertToMediaVideoBuffer(
    crosapi::mojom::VideoBufferHandlePtr buffer_handle) {
  if (buffer_handle->is_shared_buffer_handle()) {
    // TODO(crbug.com/40218955): The LaCrOS interface should be migrated
    // to use base::UnsafeSharedMemoryRegion as well.
    return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
        base::UnsafeSharedMemoryRegion::Deserialize(
            mojo::UnwrapPlatformSharedMemoryRegion(
                std::move(buffer_handle->get_shared_buffer_handle()))));
  } else if (buffer_handle->is_gpu_memory_buffer_handle()) {
    return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
        ToGfxGpuMemoryBufferHandle(
            std::move(buffer_handle->get_gpu_memory_buffer_handle())));
  } else if (buffer_handle->is_read_only_shmem_region()) {
    return media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
        std::move(buffer_handle->get_read_only_shmem_region()));
  } else {
    NOTREACHED_IN_MIGRATION() << "Unexpected new buffer type";
  }

  return nullptr;
}

media::mojom::VideoFrameInfoPtr ConvertToMediaVideoFrameInfo(
    crosapi::mojom::VideoFrameInfoPtr buffer_info) {
  auto video_capture_buffer_info = media::mojom::VideoFrameInfo::New();
  video_capture_buffer_info->timestamp = buffer_info->timestamp;
  video_capture_buffer_info->pixel_format = buffer_info->pixel_format;
  video_capture_buffer_info->coded_size = buffer_info->coded_size;
  video_capture_buffer_info->visible_rect = buffer_info->visible_rect;

  media::VideoFrameMetadata media_frame_metadata;
  switch (buffer_info->rotation) {
    case crosapi::mojom::VideoRotation::kVideoRotation0:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_0);
      break;
    case crosapi::mojom::VideoRotation::kVideoRotation90:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_90);
      break;
    case crosapi::mojom::VideoRotation::kVideoRotation180:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_180);
      break;
    case crosapi::mojom::VideoRotation::kVideoRotation270:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_270);
      break;
  }
  media_frame_metadata.reference_time = buffer_info->reference_time;

  video_capture_buffer_info->metadata = std::move(media_frame_metadata);

  return video_capture_buffer_info;
}

media::ReadyFrameInBuffer ConvertToMediaReadyFrame(
    crosapi::mojom::ReadyFrameInBufferPtr buffer) {
  return media::ReadyFrameInBuffer(
      buffer->buffer_id, buffer->frame_feedback_id,
      GetAccessPermissionHelper(std::move(buffer->access_permission)),
      ConvertToMediaVideoFrameInfo(std::move(buffer->frame_info)));
}

}  // namespace video_capture
