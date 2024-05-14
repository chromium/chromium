// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/ash/video_frame_handler_ash.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "media/base/video_transformation.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace crosapi {

namespace {

crosapi::mojom::ReadyFrameInBufferPtr ToCrosapiBuffer(
    video_capture::mojom::ReadyFrameInBufferPtr buffer,
    scoped_refptr<video_capture::VideoFrameAccessHandlerRemote>
        frame_access_handler_remote) {
  auto crosapi_buffer = crosapi::mojom::ReadyFrameInBuffer::New();
  crosapi_buffer->buffer_id = buffer->buffer_id;
  crosapi_buffer->frame_feedback_id = buffer->frame_feedback_id;

  mojo::PendingRemote<crosapi::mojom::ScopedAccessPermission> access_permission;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoFrameHandlerAsh::ScopedFrameAccessHandlerNotifier>(
          frame_access_handler_remote, buffer->buffer_id),
      access_permission.InitWithNewPipeAndPassReceiver());
  crosapi_buffer->access_permission = std::move(access_permission);

  const auto& buffer_info = buffer->frame_info;
  auto crosapi_buffer_info = crosapi::mojom::VideoFrameInfo::New();
  crosapi_buffer_info->timestamp = buffer_info->timestamp;
  crosapi_buffer_info->pixel_format = buffer_info->pixel_format;
  crosapi_buffer_info->coded_size = buffer_info->coded_size;
  crosapi_buffer_info->visible_rect = buffer_info->visible_rect;

  auto transformation = buffer_info->metadata.transformation;
  if (transformation) {
    crosapi::mojom::VideoRotation crosapi_rotation;
    switch (transformation->rotation) {
      case media::VideoRotation::VIDEO_ROTATION_0:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation0;
        break;
      case media::VideoRotation::VIDEO_ROTATION_90:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation90;
        break;
      case media::VideoRotation::VIDEO_ROTATION_180:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation180;
        break;
      case media::VideoRotation::VIDEO_ROTATION_270:
        crosapi_rotation = crosapi::mojom::VideoRotation::kVideoRotation270;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unexpected rotation in video frame metadata";
    }
    crosapi_buffer_info->rotation = crosapi_rotation;
  }
  if (buffer_info->metadata.reference_time.has_value()) {
    crosapi_buffer_info->reference_time = *buffer_info->metadata.reference_time;
  }

  crosapi_buffer->frame_info = std::move(crosapi_buffer_info);
  return crosapi_buffer;
}

crosapi::mojom::GpuMemoryBufferHandlePtr ToCrosapiGpuMemoryBufferHandle(
    gfx::GpuMemoryBufferHandle buffer_handle) {
  auto crosapi_gpu_handle = crosapi::mojom::GpuMemoryBufferHandle::New();
  crosapi_gpu_handle->id = buffer_handle.id.id;
  crosapi_gpu_handle->offset = buffer_handle.offset;
  crosapi_gpu_handle->stride = buffer_handle.stride;

  if (buffer_handle.type == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER) {
    crosapi_gpu_handle->platform_handle =
        crosapi::mojom::GpuMemoryBufferPlatformHandle::NewSharedMemoryHandle(
            std::move(buffer_handle.region));
  } else if (buffer_handle.type == gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    auto crosapi_native_pixmap_handle =
        crosapi::mojom::NativePixmapHandle::New();
    crosapi_native_pixmap_handle->planes =
        std::move(buffer_handle.native_pixmap_handle.planes);
    crosapi_native_pixmap_handle->modifier =
        buffer_handle.native_pixmap_handle.modifier;
    crosapi_gpu_handle->platform_handle =
        crosapi::mojom::GpuMemoryBufferPlatformHandle::NewNativePixmapHandle(
            std::move(crosapi_native_pixmap_handle));
  }
  return crosapi_gpu_handle;
}

}  // namespace

VideoFrameHandlerAsh::VideoFrameHandlerAsh(
    mojo::PendingReceiver<video_capture::mojom::VideoFrameHandler>
        handler_receiver,
    mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_remote)
    : proxy_(std::move(proxy_remote)) {
  receiver_.Bind(std::move(handler_receiver));
}

VideoFrameHandlerAsh::~VideoFrameHandlerAsh() = default;

VideoFrameHandlerAsh::ScopedFrameAccessHandlerNotifier::
    ScopedFrameAccessHandlerNotifier(
        scoped_refptr<video_capture::VideoFrameAccessHandlerRemote>
            frame_access_handler_remote,
        int32_t buffer_id)
    : frame_access_handler_remote_(std::move(frame_access_handler_remote)),
      buffer_id_(buffer_id) {}

VideoFrameHandlerAsh::ScopedFrameAccessHandlerNotifier::
    ~ScopedFrameAccessHandlerNotifier() {
  (*frame_access_handler_remote_)->OnFinishedConsumingBuffer(buffer_id_);
}

void VideoFrameHandlerAsh::OnCaptureConfigurationChanged() {
  proxy_->OnCaptureConfigurationChanged();
}

void VideoFrameHandlerAsh::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  crosapi::mojom::VideoBufferHandlePtr crosapi_handle;

  if (buffer_handle->is_unsafe_shmem_region()) {
    crosapi_handle = crosapi::mojom::VideoBufferHandle::NewSharedBufferHandle(
        mojo::WrapPlatformSharedMemoryRegion(
            base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
                std::move(buffer_handle->get_unsafe_shmem_region()))));
  } else if (buffer_handle->is_gpu_memory_buffer_handle()) {
    crosapi_handle =
        crosapi::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
            ToCrosapiGpuMemoryBufferHandle(
                std::move(buffer_handle->get_gpu_memory_buffer_handle())));
  } else if (buffer_handle->is_read_only_shmem_region()) {
    // Lacros is guaranteed to be newer than us so it's okay to skip the version
    // check here.
    crosapi_handle = crosapi::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
        std::move(buffer_handle->get_read_only_shmem_region()));
  } else {
    NOTREACHED_IN_MIGRATION() << "Unexpected new buffer type";
  }
  proxy_->OnNewBuffer(buffer_id, std::move(crosapi_handle));
}

void VideoFrameHandlerAsh::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  DCHECK(!frame_access_handler_remote_);
  frame_access_handler_remote_ =
      base::MakeRefCounted<video_capture::VideoFrameAccessHandlerRemote>(
          mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>(
              std::move(pending_frame_access_handler)));
}

void VideoFrameHandlerAsh::OnFrameReadyInBuffer(
    video_capture::mojom::ReadyFrameInBufferPtr buffer) {
  DCHECK(frame_access_handler_remote_);
  crosapi::mojom::ReadyFrameInBufferPtr crosapi_buffer =
      ToCrosapiBuffer(std::move(buffer), frame_access_handler_remote_);

  proxy_->OnFrameReadyInBuffer(std::move(crosapi_buffer));
}

void VideoFrameHandlerAsh::OnBufferRetired(int buffer_id) {
  proxy_->OnBufferRetired(buffer_id);
}

void VideoFrameHandlerAsh::OnError(media::VideoCaptureError error) {
  proxy_->OnError(error);
}

void VideoFrameHandlerAsh::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  proxy_->OnFrameDropped(reason);
}

void VideoFrameHandlerAsh::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  proxy_->OnNewSubCaptureTargetVersion(sub_capture_target_version);
}

void VideoFrameHandlerAsh::OnFrameWithEmptyRegionCapture() {
  proxy_->OnFrameWithEmptyRegionCapture();
}

void VideoFrameHandlerAsh::OnLog(const std::string& message) {
  proxy_->OnLog(message);
}

void VideoFrameHandlerAsh::OnStarted() {
  proxy_->OnStarted();
}

void VideoFrameHandlerAsh::OnStartedUsingGpuDecode() {
  proxy_->OnStartedUsingGpuDecode();
}

void VideoFrameHandlerAsh::OnStopped() {
  proxy_->OnStopped();
}

}  // namespace crosapi
