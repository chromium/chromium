// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FAKE_GPU_MEMORY_BUFFER_H_
#define MEDIA_VIDEO_FAKE_GPU_MEMORY_BUFFER_H_

#include <memory>

#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/video_types.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

// A fake implementation of gfx::GpuMemoryBuffer for testing purposes.
class FakeGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  // Used to defer execution of `MapAsync()` result callback.
  // FakeGpuMemoryBuffer will pass the  callbacks to the configured instance
  // of this class, which can execute them as the test requires.
  class MapCallbackController {
   public:
    virtual void RegisterCallback(base::OnceCallback<void(bool)> result_cb) = 0;
  };

  FakeGpuMemoryBuffer() = delete;

  FakeGpuMemoryBuffer(const gfx::Size& size, gfx::BufferFormat format);
  FakeGpuMemoryBuffer(const gfx::Size& size,
                      gfx::BufferFormat format,
                      uint64_t modifier);
  FakeGpuMemoryBuffer(const gfx::Size& size,
                      gfx::BufferFormat format,
                      bool premapped,
                      MapCallbackController* controller);

  FakeGpuMemoryBuffer(const FakeGpuMemoryBuffer&) = delete;
  FakeGpuMemoryBuffer& operator=(const FakeGpuMemoryBuffer&) = delete;

  // gfx::GpuMemoryBuffer implementation.
  ~FakeGpuMemoryBuffer() override;
  bool Map() override;
  void MapAsync(base::OnceCallback<void(bool)> result_cb) override;
  bool AsyncMappingIsNonBlocking() const override;
  void* memory(size_t plane) override;
  void Unmap() override;
  gfx::Size GetSize() const override;
  gfx::BufferFormat GetFormat() const override;
  int stride(size_t plane) const override;
  gfx::GpuMemoryBufferId GetId() const override;
  gfx::GpuMemoryBufferType GetType() const override;
  gfx::GpuMemoryBufferHandle CloneHandle() const override;
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override;

 private:
  gfx::Size size_;
  gfx::BufferFormat format_;
  VideoPixelFormat video_pixel_format_ = PIXEL_FORMAT_UNKNOWN;
  std::vector<uint8_t> data_;
  gfx::GpuMemoryBufferHandle handle_;
  bool premapped_ = true;
  raw_ptr<MapCallbackController> map_callback_controller_ = nullptr;
};

class FakeGpuMemoryBufferSupport : public gpu::GpuMemoryBufferSupport {
 public:
  std::unique_ptr<gpu::GpuMemoryBufferImpl> CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback callback,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr,
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr,
      base::span<uint8_t> premapped_memory = base::span<uint8_t>()) override;
};

}  // namespace media

#endif  // MEDIA_VIDEO_FAKE_GPU_MEMORY_BUFFER_H_
