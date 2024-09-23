// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/video/mock_gpu_video_accelerator_factories.h"

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

base::AtomicSequenceNumber g_gpu_memory_buffer_id_generator;

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size,
                      gfx::BufferFormat format,
                      bool fail_to_map_gpu_memory_buffer)
      : mapped_(false),
        format_(format),
        size_(size),
        num_planes_(gfx::NumberOfPlanesForLinearBufferFormat(format)),
        id_(g_gpu_memory_buffer_id_generator.GetNext() + 1),
        fail_to_map_gpu_memory_buffer_(fail_to_map_gpu_memory_buffer) {
    DCHECK(gfx::BufferFormat::R_8 == format_ ||
           gfx::BufferFormat::RG_88 == format_ ||
           gfx::BufferFormat::YUV_420_BIPLANAR == format_ ||
           gfx::BufferFormat::P010 == format_ ||
           gfx::BufferFormat::BGRA_1010102 == format_ ||
           gfx::BufferFormat::RGBA_1010102 == format_ ||
           gfx::BufferFormat::RGBA_8888 == format_ ||
           gfx::BufferFormat::BGRA_8888 == format_);
    DCHECK(num_planes_ <= kMaxPlanes);
    for (int i = 0; i < static_cast<int>(num_planes_); ++i) {
      bytes_[i].resize(gfx::PlaneSizeForBufferFormat(size_, format_, i));
    }
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    if (fail_to_map_gpu_memory_buffer_)
      return false;
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, num_planes_);
    return &bytes_[plane][0];
  }
  void Unmap() override {
    if (fail_to_map_gpu_memory_buffer_)
      return;
    DCHECK(mapped_);
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, num_planes_);
    return static_cast<int>(gfx::RowSizeForBufferFormat(
        size_.width(), format_, static_cast<int>(plane)));
  }
  gfx::GpuMemoryBufferId GetId() const override { return id_; }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::SHARED_MEMORY_BUFFER;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    return gfx::GpuMemoryBufferHandle();
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

 private:
  static const size_t kMaxPlanes = 3;

  bool mapped_;
  gfx::BufferFormat format_;
  const gfx::Size size_;
  size_t num_planes_;
  std::vector<uint8_t> bytes_[kMaxPlanes];
  gfx::GpuMemoryBufferId id_;
  bool fail_to_map_gpu_memory_buffer_ = false;
};

}  // unnamed namespace

MockGpuVideoAcceleratorFactories::MockGpuVideoAcceleratorFactories(
    gpu::SharedImageInterface* sii)
    : sii_(sii) {}

MockGpuVideoAcceleratorFactories::~MockGpuVideoAcceleratorFactories() = default;

bool MockGpuVideoAcceleratorFactories::IsGpuVideoDecodeAcceleratorEnabled() {
  return true;
}

bool MockGpuVideoAcceleratorFactories::IsGpuVideoEncodeAcceleratorEnabled() {
  return true;
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MockGpuVideoAcceleratorFactories::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage /* usage */) {
  base::AutoLock guard(lock_);
  if (fail_to_allocate_gpu_memory_buffer_)
    return nullptr;
  auto ret = std::make_unique<GpuMemoryBufferImpl>(
      size, format, fail_to_map_gpu_memory_buffer_);
  created_memory_buffers_.push_back(ret.get());
  return ret;
}

base::UnsafeSharedMemoryRegion
MockGpuVideoAcceleratorFactories::CreateSharedMemoryRegion(size_t size) {
  return base::UnsafeSharedMemoryRegion::Create(size);
}

std::unique_ptr<VideoEncodeAccelerator>
MockGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return base::WrapUnique(DoCreateVideoEncodeAccelerator());
}

bool MockGpuVideoAcceleratorFactories::ShouldUseGpuMemoryBuffersForVideoFrames(
    bool for_media_stream) const {
  return false;
}

}  // namespace media
