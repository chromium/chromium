// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_gpu_memory_buffer_manager.h"
#include "build/chromeos_buildflags.h"

#include "media/video/fake_gpu_memory_buffer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event) {
  auto gmb = std::make_unique<FakeGpuMemoryBuffer>(size, format);
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
