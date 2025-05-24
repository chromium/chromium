// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/fake_gpu_memory_buffer.h"

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

namespace gpu {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDWR));
  DCHECK(fd.is_valid());
  return fd;
}
#endif

static base::AtomicSequenceNumber buffer_id_generator;

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
gfx::GpuMemoryBufferHandle CreatePixmapHandleForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint64_t modifier) {
  std::optional<media::VideoPixelFormat> video_pixel_format =
      media::GfxBufferFormatToVideoPixelFormat(format);
  CHECK(video_pixel_format);

  gfx::NativePixmapHandle native_pixmap_handle;
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(*video_pixel_format);
       i++) {
    const gfx::Size plane_size_in_bytes =
        media::VideoFrame::PlaneSize(*video_pixel_format, i, size);
    native_pixmap_handle.planes.emplace_back(plane_size_in_bytes.width(), 0,
                                             plane_size_in_bytes.GetArea(),
                                             GetDummyFD());
  }
  native_pixmap_handle.modifier = modifier;

  gfx::GpuMemoryBufferHandle handle(std::move(native_pixmap_handle));
  handle.id = gfx::GpuMemoryBufferId(buffer_id_generator.GetNext());
  return handle;
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
  std::optional<media::VideoPixelFormat> video_pixel_format =
      media::GfxBufferFormatToVideoPixelFormat(format);
  CHECK(video_pixel_format);
  video_pixel_format_ = *video_pixel_format;

  const size_t allocation_size =
      media::VideoFrame::AllocationSize(video_pixel_format_, size_);
  data_ = std::vector<uint8_t>(allocation_size);

  handle_.type = gfx::SHARED_MEMORY_BUFFER;
  handle_.id = gfx::GpuMemoryBufferId(buffer_id_generator.GetNext());
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
  DCHECK_LT(plane, media::VideoFrame::NumPlanes(video_pixel_format_));
  auto* data_ptr = data_.data();
  for (size_t i = 1; i <= plane; i++) {
    data_ptr += media::VideoFrame::PlaneSize(video_pixel_format_, i - 1, size_)
                    .GetArea();
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
  DCHECK_LT(plane, media::VideoFrame::NumPlanes(video_pixel_format_));
  return media::VideoFrame::PlaneSize(video_pixel_format_, plane, size_)
      .width();
}

gfx::GpuMemoryBufferId FakeGpuMemoryBuffer::GetId() const {
  return handle_.id;
}

gfx::GpuMemoryBufferType FakeGpuMemoryBuffer::GetType() const {
  return gfx::SHARED_MEMORY_BUFFER;
}

gfx::GpuMemoryBufferHandle FakeGpuMemoryBuffer::CloneHandle() const {
  return handle_.Clone();
}

void FakeGpuMemoryBuffer::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) const {}

}  // namespace gpu
