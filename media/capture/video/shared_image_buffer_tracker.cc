// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/shared_image_buffer_tracker.h"

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/ipc/common/exported_shared_image.mojom.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"

namespace media {

SharedImageBufferTracker::SharedImageBufferTracker(
    scoped_refptr<gpu::ClientSharedImage> client_shared_image)
    : client_shared_image_(std::move(client_shared_image)) {}
SharedImageBufferTracker::~SharedImageBufferTracker() = default;

bool SharedImageBufferTracker::Init(const gfx::Size& dimensions,
                                    VideoPixelFormat format,
                                    const mojom::PlaneStridesPtr& strides) {
  if (!client_shared_image_) {
    return false;
  }
  if (client_shared_image_->size() != dimensions) {
    return false;
  }
  return true;
}

bool SharedImageBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  return false;
}

uint32_t SharedImageBufferTracker::GetMemorySizeInBytes() {
  if (client_shared_image_) {
    return base::checked_cast<uint32_t>(
        client_shared_image_->EstimatedSizeInBytes().InBytes());
  }
  return 0;
}

std::unique_ptr<VideoCaptureBufferHandle>
SharedImageBufferTracker::GetMemoryMappedAccess() {
  NOTREACHED() << "Unsupported operation";
}

base::UnsafeSharedMemoryRegion
SharedImageBufferTracker::DuplicateAsUnsafeRegion() {
  NOTREACHED() << "Unsupported operation";
}

gfx::GpuMemoryBufferHandle
SharedImageBufferTracker::GetGpuMemoryBufferHandle() {
  NOTREACHED() << "Unsupported operation";
}

media::mojom::VideoBufferHandlePtr
SharedImageBufferTracker::GetVideoBufferHandle() {
  if (!client_shared_image_) {
    return nullptr;
  }
  auto exported_shared_image = client_shared_image_->Export();
  auto handle_set = media::mojom::SharedImageBufferHandleSet::New(
      std::move(exported_shared_image),
      client_shared_image_->creation_sync_token());
  return media::mojom::VideoBufferHandle::NewSharedImageHandle(
      std::move(handle_set));
}

VideoCaptureBufferType SharedImageBufferTracker::GetBufferType() {
  return VideoCaptureBufferType::kSharedImage;
}

void SharedImageBufferTracker::UpdateExternalData(
    CapturedExternalVideoBuffer buffer) {
  client_shared_image_ = std::move(buffer.client_shared_image);
}

}  // namespace media
