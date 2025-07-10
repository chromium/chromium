// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
namespace {

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size,
                      gfx::BufferFormat format,
                      base::UnsafeSharedMemoryRegion shared_memory_region,
                      size_t offset,
                      size_t stride)
      : size_(size),
        format_(format),
        region_(std::move(shared_memory_region)),
        offset_(offset),
        stride_(stride),
        mapped_(false) {}

  ~GpuMemoryBufferImpl() override = default;

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    DCHECK_EQ(stride_, gfx::RowSizeForBufferFormat(size_.width(), format_, 0));
    mapping_ = region_.MapAt(
        0, offset_ + gfx::BufferSizeForBufferFormat(size_, format_));
    if (!mapping_.IsValid())
      return false;
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return reinterpret_cast<uint8_t*>(mapping_.memory()) + offset_ +
           gfx::BufferOffsetForBufferFormat(size_, format_, plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapping_ = base::WritableSharedMemoryMapping();
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return base::checked_cast<int>(gfx::RowSizeForBufferFormat(
        size_.width(), format_, static_cast<int>(plane)));
  }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::SHARED_MEMORY_BUFFER;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    gfx::GpuMemoryBufferHandle handle(region_.Duplicate());
    handle.offset = base::checked_cast<uint32_t>(offset_);
    handle.stride = base::checked_cast<uint32_t>(stride_);
    return handle;
  }

 private:
  const gfx::Size size_;
  gfx::BufferFormat format_;
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
  size_t offset_;
  size_t stride_;
  bool mapped_;
};

}  // namespace

TestGpuMemoryBufferManager::TestGpuMemoryBufferManager() = default;

TestGpuMemoryBufferManager::~TestGpuMemoryBufferManager() = default;

std::unique_ptr<gfx::GpuMemoryBuffer>
TestGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event) {
  base::AutoLock hold(lock_);

  const size_t buffer_size = gfx::BufferSizeForBufferFormat(size, format);
  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!shared_memory_region.IsValid())
    return nullptr;

  std::unique_ptr<gfx::GpuMemoryBuffer> result(new GpuMemoryBufferImpl(
      size, format, std::move(shared_memory_region), 0,
      base::checked_cast<int>(
          gfx::RowSizeForBufferFormat(size.width(), format, 0))));
  return result;
}

}  // namespace gpu
