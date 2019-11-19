// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/mock_gpu_video_accelerator_factories.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

int g_next_gpu_memory_buffer_id = 1;

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size, gfx::BufferFormat format)
      : mapped_(false),
        format_(format),
        size_(size),
        num_planes_(gfx::NumberOfPlanesForLinearBufferFormat(format)),
        id_(g_next_gpu_memory_buffer_id++) {
    DCHECK(gfx::BufferFormat::R_8 == format_ ||
           gfx::BufferFormat::RG_88 == format_ ||
           gfx::BufferFormat::YUV_420_BIPLANAR == format_ ||
           gfx::BufferFormat::BGRX_1010102 == format_ ||
           gfx::BufferFormat::RGBX_1010102 == format_ ||
           gfx::BufferFormat::RGBA_8888 == format_ ||
           gfx::BufferFormat::BGRA_8888 == format_);
    DCHECK(num_planes_ <= kMaxPlanes);
    for (int i = 0; i < static_cast<int>(num_planes_); ++i) {
      bytes_[i].resize(gfx::RowSizeForBufferFormat(size_.width(), format_, i) *
                       size_.height() /
                       gfx::SubsamplingFactorForBufferFormat(format_, i));
    }
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
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
    return gfx::NATIVE_PIXMAP;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
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
};

}  // unnamed namespace

MockGpuVideoAcceleratorFactories::MockGpuVideoAcceleratorFactories(
    gpu::SharedImageInterface* sii)
    : sii_(sii) {}

MockGpuVideoAcceleratorFactories::~MockGpuVideoAcceleratorFactories() = default;

bool MockGpuVideoAcceleratorFactories::IsGpuVideoAcceleratorEnabled() {
  return true;
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MockGpuVideoAcceleratorFactories::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage /* usage */) {
  if (fail_to_allocate_gpu_memory_buffer_)
    return nullptr;
  std::unique_ptr<gfx::GpuMemoryBuffer> ret(
      new GpuMemoryBufferImpl(size, format));
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

unsigned MockGpuVideoAcceleratorFactories::ImageTextureTarget(
    gfx::BufferFormat format) {
  return GL_TEXTURE_2D;
}

}  // namespace media
