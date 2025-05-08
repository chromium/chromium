// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_FAKE_GPU_MEMORY_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_FAKE_GPU_MEMORY_BUFFER_H_

#include <memory>

#include "media/base/video_types.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// This method is used by tests to create a fake pixmap handle instead of
// creating a FakeGpuMemoryBuffer. Once all tests are converted to use it,
// FakeGpuMemoryBuffer will be removed and this file will be renamed
// appropriately. Note that this method is only exposed to linux and chromeos
// whereas the FakeGpuMemoryBuffer itself can be used in any platform as of now
// with a handle type of gfx::NATIVE_PIXMAP which is confusing. Removing
// and replacing FakeGpuMemoryBuffer with platform specific handle creation
// methods will address those concerns.
gfx::GpuMemoryBufferHandle CreatePixmapHandleForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint64_t modifier = gfx::NativePixmapHandle::kNoModifier);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

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
  media::VideoPixelFormat video_pixel_format_ = media::PIXEL_FORMAT_UNKNOWN;
  std::vector<uint8_t> data_;
  gfx::GpuMemoryBufferHandle handle_;
  bool premapped_ = true;
  raw_ptr<MapCallbackController> map_callback_controller_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_FAKE_GPU_MEMORY_BUFFER_H_
