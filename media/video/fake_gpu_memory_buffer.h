// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FAKE_GPU_MEMORY_BUFFER_H_
#define MEDIA_VIDEO_FAKE_GPU_MEMORY_BUFFER_H_

#include <memory>

#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

// A fake implementation of gfx::GpuMemoryBuffer for testing purposes.
class FakeGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  FakeGpuMemoryBuffer(const gfx::Size& size, gfx::BufferFormat format);

  // gfx::GpuMemoryBuffer implementation.
  ~FakeGpuMemoryBuffer() override;
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
  ClientBuffer AsClientBuffer() override;
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override;

 private:
  gfx::Size size_;
  gfx::BufferFormat format_;
  std::vector<uint8_t> data_;
  gfx::GpuMemoryBufferHandle handle_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(FakeGpuMemoryBuffer);
};

class FakeGpuMemoryBufferSupport : public gpu::GpuMemoryBufferSupport {
 public:
  std::unique_ptr<gpu::GpuMemoryBufferImpl> CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback callback) override;
};

}  // namespace media

#endif  // MEDIA_VIDEO_FAKE_GPU_MEMORY_BUFFER_H_
