// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/video/fake_gpu_memory_buffer.h"

#include "base/atomic_sequence_num.h"
#include "build/build_config.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/eventpair.h>
#include <lib/zx/object.h>
#endif

namespace media {

namespace {

class FakeGpuMemoryBufferImpl : public gpu::GpuMemoryBufferImpl {
 public:
  FakeGpuMemoryBufferImpl(const gfx::Size& size, gfx::BufferFormat format)
      : gpu::GpuMemoryBufferImpl(
            gfx::GpuMemoryBufferId(),
            size,
            format,
            gpu::GpuMemoryBufferImpl::DestructionCallback()),
        fake_gmb_(std::make_unique<media::FakeGpuMemoryBuffer>(size, format)) {}

  // gfx::GpuMemoryBuffer implementation
  bool Map() override { return fake_gmb_->Map(); }
  void MapAsync(base::OnceCallback<void(bool)> result_cb) override {
    fake_gmb_->MapAsync(std::move(result_cb));
  }
  bool AsyncMappingIsNonBlocking() const override {
    return fake_gmb_->AsyncMappingIsNonBlocking();
  }
  void* memory(size_t plane) override { return fake_gmb_->memory(plane); }
  void Unmap() override { fake_gmb_->Unmap(); }
  int stride(size_t plane) const override { return fake_gmb_->stride(plane); }
  gfx::GpuMemoryBufferType GetType() const override {
    return fake_gmb_->GetType();
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    return fake_gmb_->CloneHandle();
  }

 private:
  std::unique_ptr<media::FakeGpuMemoryBuffer> fake_gmb_;
};

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDWR));
  DCHECK(fd.is_valid());
  return fd;
}
#endif

FakeGpuMemoryBuffer::FakeGpuMemoryBuffer(const gfx::Size& size,
                                         gfx::BufferFormat format)
    : FakeGpuMemoryBuffer(size, format, gfx::NativePixmapHandle::kNoModifier) {}

FakeGpuMemoryBuffer::FakeGpuMemoryBuffer(const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         bool premapped,
                                         MapCallbackController* controller)
    : FakeGpuMemoryBuffer(size, format, gfx::NativePixmapHandle::kNoModifier) {
  premapped_ = premapped;
  map_callback_controller_ = controller;
}

FakeGpuMemoryBuffer::FakeGpuMemoryBuffer(const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         uint64_t modifier)
    : size_(size), format_(format) {
  std::optional<VideoPixelFormat> video_pixel_format =
      GfxBufferFormatToVideoPixelFormat(format);
  CHECK(video_pixel_format);
  video_pixel_format_ = *video_pixel_format;

  const size_t allocation_size =
      VideoFrame::AllocationSize(video_pixel_format_, size_);
  data_ = std::vector<uint8_t>(allocation_size);

  handle_.type = gfx::NATIVE_PIXMAP;

  static base::AtomicSequenceNumber buffer_id_generator;
  handle_.id = gfx::GpuMemoryBufferId(buffer_id_generator.GetNext());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  for (size_t i = 0; i < VideoFrame::NumPlanes(video_pixel_format_); i++) {
    const gfx::Size plane_size_in_bytes =
        VideoFrame::PlaneSize(video_pixel_format_, i, size_);
    handle_.native_pixmap_handle.planes.emplace_back(
        plane_size_in_bytes.width(), 0, plane_size_in_bytes.GetArea(),
        GetDummyFD());
  }
  handle_.native_pixmap_handle.modifier = modifier;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_FUCHSIA)
  zx::eventpair client_handle, service_handle;
  zx::eventpair::create(0, &client_handle, &service_handle);
  handle_.native_pixmap_handle.buffer_collection_handle =
      std::move(client_handle);
#endif
}

FakeGpuMemoryBuffer::~FakeGpuMemoryBuffer() = default;

bool FakeGpuMemoryBuffer::Map() {
  return true;
}

void FakeGpuMemoryBuffer::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  if (premapped_) {
    std::move(result_cb).Run(true);
    return;
  }
  map_callback_controller_->RegisterCallback(std::move(result_cb));
}

bool FakeGpuMemoryBuffer::AsyncMappingIsNonBlocking() const {
  return true;
}

void* FakeGpuMemoryBuffer::memory(size_t plane) {
  DCHECK_LT(plane, VideoFrame::NumPlanes(video_pixel_format_));
  auto* data_ptr = data_.data();
  for (size_t i = 1; i <= plane; i++) {
    data_ptr +=
        VideoFrame::PlaneSize(video_pixel_format_, i - 1, size_).GetArea();
  }
  return data_ptr;
}

void FakeGpuMemoryBuffer::Unmap() {}

gfx::Size FakeGpuMemoryBuffer::GetSize() const {
  return size_;
}

gfx::BufferFormat FakeGpuMemoryBuffer::GetFormat() const {
  return format_;
}

int FakeGpuMemoryBuffer::stride(size_t plane) const {
  DCHECK_LT(plane, VideoFrame::NumPlanes(video_pixel_format_));
  return VideoFrame::PlaneSize(video_pixel_format_, plane, size_).width();
}

gfx::GpuMemoryBufferId FakeGpuMemoryBuffer::GetId() const {
  return handle_.id;
}

gfx::GpuMemoryBufferType FakeGpuMemoryBuffer::GetType() const {
  return gfx::NATIVE_PIXMAP;
}

gfx::GpuMemoryBufferHandle FakeGpuMemoryBuffer::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.id = handle_.id;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  handle.native_pixmap_handle =
      gfx::CloneHandleForIPC(handle_.native_pixmap_handle);
#endif
  return handle;
}

void FakeGpuMemoryBuffer::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) const {}

std::unique_ptr<gpu::GpuMemoryBufferImpl>
FakeGpuMemoryBufferSupport::CreateGpuMemoryBufferImplFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::GpuMemoryBufferImpl::DestructionCallback callback,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool,
    base::span<uint8_t> premapped_memory) {
  return std::make_unique<FakeGpuMemoryBufferImpl>(size, format);
}

}  // namespace media
