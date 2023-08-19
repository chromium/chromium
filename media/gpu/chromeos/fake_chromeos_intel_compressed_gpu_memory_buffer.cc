// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/fake_chromeos_intel_compressed_gpu_memory_buffer.h"

#include <drm_fourcc.h>
#include <fcntl.h>

#include "base/atomic_sequence_num.h"

namespace media {

namespace {
base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDWR));
  DCHECK(fd.is_valid());
  return fd;
}
}  // namespace

FakeChromeOSIntelCompressedGpuMemoryBuffer::
    FakeChromeOSIntelCompressedGpuMemoryBuffer(const gfx::Size& size,
                                               gfx::BufferFormat format)
    : size_(size), format_(format) {
  CHECK(format == gfx::BufferFormat::YUV_420_BIPLANAR ||
        format == gfx::BufferFormat::P010);

  handle_.type = gfx::NATIVE_PIXMAP;

  static base::AtomicSequenceNumber buffer_id_generator;
  handle_.id = gfx::GpuMemoryBufferId(buffer_id_generator.GetNext());

  constexpr size_t kExpectedNumberOfPlanes = 4u;
  // TODO(b/281907962): consider using real strides, offsets, and plane sizes.
  // For now, that's not necessary for the testing we do.
  for (size_t i = 0; i < kExpectedNumberOfPlanes; i++) {
    handle_.native_pixmap_handle.planes.emplace_back(
        /*stride=*/i,
        /*offset=*/i,
        /*size=*/i, GetDummyFD());
  }
  handle_.native_pixmap_handle.modifier = I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS;
}

FakeChromeOSIntelCompressedGpuMemoryBuffer::
    ~FakeChromeOSIntelCompressedGpuMemoryBuffer() = default;

bool FakeChromeOSIntelCompressedGpuMemoryBuffer::Map() {
  return false;
}

void* FakeChromeOSIntelCompressedGpuMemoryBuffer::memory(size_t plane) {
  return nullptr;
}

void FakeChromeOSIntelCompressedGpuMemoryBuffer::Unmap() {}

gfx::Size FakeChromeOSIntelCompressedGpuMemoryBuffer::GetSize() const {
  return size_;
}

gfx::BufferFormat FakeChromeOSIntelCompressedGpuMemoryBuffer::GetFormat()
    const {
  return format_;
}

int FakeChromeOSIntelCompressedGpuMemoryBuffer::stride(size_t plane) const {
  return handle_.native_pixmap_handle.planes[plane].stride;
}

void FakeChromeOSIntelCompressedGpuMemoryBuffer::SetColorSpace(
    const gfx::ColorSpace& color_space) {}

gfx::GpuMemoryBufferId FakeChromeOSIntelCompressedGpuMemoryBuffer::GetId()
    const {
  return handle_.id;
}

gfx::GpuMemoryBufferType FakeChromeOSIntelCompressedGpuMemoryBuffer::GetType()
    const {
  return gfx::NATIVE_PIXMAP;
}

gfx::GpuMemoryBufferHandle
FakeChromeOSIntelCompressedGpuMemoryBuffer::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.id = handle_.id;
  handle.native_pixmap_handle =
      gfx::CloneHandleForIPC(handle_.native_pixmap_handle);
  return handle;
}

void FakeChromeOSIntelCompressedGpuMemoryBuffer::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) const {}

uint64_t FakeChromeOSIntelCompressedGpuMemoryBuffer::plane_offset(
    size_t plane) const {
  return handle_.native_pixmap_handle.planes[plane].offset;
}

uint64_t FakeChromeOSIntelCompressedGpuMemoryBuffer::plane_size(
    size_t plane) const {
  return handle_.native_pixmap_handle.planes[plane].size;
}

}  // namespace media
