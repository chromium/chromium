// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_FAKE_CHROMEOS_INTEL_COMPRESSED_GPU_MEMORY_BUFFER_H_
#define MEDIA_GPU_CHROMEOS_FAKE_CHROMEOS_INTEL_COMPRESSED_GPU_MEMORY_BUFFER_H_

#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

// A fake implementation of gpu::GpuMemoryBuffer for
// testing purposes. It emulates a GpuMemoryBuffer that references a dma-buf
// that uses Intel media compression (with a modifier of either
// I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS or I915_FORMAT_MOD_4_TILED_MTL_MC_CCS).
class FakeChromeOSIntelCompressedGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  FakeChromeOSIntelCompressedGpuMemoryBuffer(const gfx::Size& size,
                                             gfx::BufferFormat format,
                                             uint64_t modifier);

  FakeChromeOSIntelCompressedGpuMemoryBuffer(
      const FakeChromeOSIntelCompressedGpuMemoryBuffer&) = delete;
  FakeChromeOSIntelCompressedGpuMemoryBuffer& operator=(
      const FakeChromeOSIntelCompressedGpuMemoryBuffer&) = delete;

  ~FakeChromeOSIntelCompressedGpuMemoryBuffer() override;

  uint64_t plane_offset(size_t plane) const;
  uint64_t plane_size(size_t plane) const;

  // gfx::GpuMemoryBuffer implementation.
  bool Map() override;
  void* memory(size_t plane) override;
  void Unmap() override;
  gfx::Size GetSize() const override;
  gfx::BufferFormat GetFormat() const override;
  int stride(size_t plane) const override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override;
  gfx::GpuMemoryBufferId GetId() const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override;

 private:
  const gfx::Size size_;
  const gfx::BufferFormat format_;
  gfx::GpuMemoryBufferHandle handle_;
};
}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_FAKE_CHROMEOS_INTEL_COMPRESSED_GPU_MEMORY_BUFFER_H_
