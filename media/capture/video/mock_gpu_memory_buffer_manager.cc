// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_gpu_memory_buffer_manager.h"

#include "media/video/fake_gpu_memory_buffer.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/request_manager.h"
#endif

namespace media {
namespace unittest_internal {

MockGpuMemoryBufferManager::MockGpuMemoryBufferManager() = default;

MockGpuMemoryBufferManager::~MockGpuMemoryBufferManager() = default;

// static
std::unique_ptr<gfx::GpuMemoryBuffer>
MockGpuMemoryBufferManager::CreateFakeGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  auto gmb = std::make_unique<FakeGpuMemoryBuffer>(size, format);
#if defined(OS_CHROMEOS)
  // For faking a valid JPEG blob buffer.
  if (base::checked_cast<size_t>(size.width()) >= sizeof(Camera3JpegBlob)) {
    Camera3JpegBlob* header = reinterpret_cast<Camera3JpegBlob*>(
        reinterpret_cast<uintptr_t>(gmb->memory(0)) + size.width() -
        sizeof(Camera3JpegBlob));
    header->jpeg_blob_id = kCamera3JpegBlobId;
    header->jpeg_size = size.width();
  }
#endif
  return gmb;
}

}  // namespace unittest_internal
}  // namespace media
