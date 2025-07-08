// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/fake_gpu_memory_buffer.h"

#include "build/build_config.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "ui/gfx/buffer_format_util.h"

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
  return handle;
}
#endif

FakeGpuMemoryBuffer::FakeGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    bool premapped,
    const ClientSharedImage::AsyncMapInvokedCallback& callback)
    : size_(size),
      format_(format),
      premapped_(premapped),
      async_map_invoked_callback_(callback) {
  int num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format_);
  size_t allocation_size = 0;
  for (int plane_index = 0; plane_index < num_planes; plane_index++) {
    size_t height_in_pixels;
    CHECK(gfx::PlaneHeightForBufferFormatChecked(
        GetSize().height(), GetFormat(), plane_index, &height_in_pixels));
    allocation_size += stride(plane_index) * height_in_pixels;
  }

  data_ = std::vector<uint8_t>(allocation_size);

  handle_.type = gfx::SHARED_MEMORY_BUFFER;
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
  async_map_invoked_callback_.Run(std::move(result_cb));
}

bool FakeGpuMemoryBuffer::AsyncMappingIsNonBlocking() const {
  return true;
}

void* FakeGpuMemoryBuffer::memory(size_t plane) {
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
  auto* data_ptr = data_.data();
  data_ptr += gfx::BufferOffsetForBufferFormat(GetSize(), GetFormat(), plane);
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
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(GetFormat()));
  return gfx::RowSizeForBufferFormat(GetSize().width(), GetFormat(), plane);
}

gfx::GpuMemoryBufferType FakeGpuMemoryBuffer::GetType() const {
  return gfx::SHARED_MEMORY_BUFFER;
}

gfx::GpuMemoryBufferHandle FakeGpuMemoryBuffer::CloneHandle() const {
  return handle_.Clone();
}

}  // namespace gpu
