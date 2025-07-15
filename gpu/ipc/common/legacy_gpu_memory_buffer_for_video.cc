// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/legacy_gpu_memory_buffer_for_video.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

LegacyGpuMemoryBufferForVideo::LegacyGpuMemoryBufferForVideo(
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::unique_ptr<gfx::ClientNativePixmap> pixmap)
    : size_(size), format_(format), pixmap_(std::move(pixmap)) {}

LegacyGpuMemoryBufferForVideo::~LegacyGpuMemoryBufferForVideo() {
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(map_lock_);
    DCHECK_EQ(map_count_, 0u);
  }
#endif
}

// static
std::unique_ptr<LegacyGpuMemoryBufferForVideo>
LegacyGpuMemoryBufferForVideo::CreateFromHandleForVideoFrame(
    gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  std::unique_ptr<gfx::ClientNativePixmap> native_pixmap =
      client_native_pixmap_factory->ImportFromHandle(
          std::move(handle).native_pixmap_handle(), size, format, usage);
  if (!native_pixmap) {
    return nullptr;
  }

  return base::WrapUnique(new LegacyGpuMemoryBufferForVideo(
      size, format, std::move(native_pixmap)));
}

bool LegacyGpuMemoryBufferForVideo::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++) {
    return true;
  }

  if (gfx::NumberOfPlanesForLinearBufferFormat(GetFormat()) !=
      pixmap_->GetNumberOfPlanes()) {
    // RGBX8888 and BGR_565 allocates 2 planes while the gfx function returns 1
    LOG(WARNING) << "Mismatched plane count "
                 << gfx::BufferFormatToString(GetFormat()) << " expected "
                 << gfx::NumberOfPlanesForLinearBufferFormat(GetFormat())
                 << " value " << pixmap_->GetNumberOfPlanes();
  }

  if (!pixmap_->Map()) {
    --map_count_;
    return false;
  }

  return true;
}

void* LegacyGpuMemoryBufferForVideo::memory(size_t plane) {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
  return pixmap_->GetMemoryAddress(plane);
}

base::span<uint8_t> LegacyGpuMemoryBufferForVideo::memory_span(size_t plane) {
  uint8_t* data = static_cast<uint8_t*>(memory(plane));
  if (!data) {
    return {};
  }
  size_t size = 0;
  if (!PlaneSizeForBufferFormatChecked(GetSize(), GetFormat(), plane, &size)) {
    return {};
  }

  return UNSAFE_BUFFERS(base::span<uint8_t>(data, size));
}

void LegacyGpuMemoryBufferForVideo::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  if (--map_count_) {
    return;
  }

  pixmap_->Unmap();
}

int LegacyGpuMemoryBufferForVideo::stride(size_t plane) const {
  // The caller is responsible for ensuring that |plane| is within bounds.
  CHECK_LT(plane, pixmap_->GetNumberOfPlanes());
  return pixmap_->GetStride(plane);
}

gfx::GpuMemoryBufferType LegacyGpuMemoryBufferForVideo::GetType() const {
  return gfx::NATIVE_PIXMAP;
}

gfx::GpuMemoryBufferHandle LegacyGpuMemoryBufferForVideo::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle(pixmap_->CloneHandleForIPC());
  return handle;
}

}  // namespace gpu
