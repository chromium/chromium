// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/internal/mappable_buffer_native_pixmap.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {
namespace {

void FreeNativePixmapForTesting(
    scoped_refptr<gfx::NativePixmap> native_pixmap) {
  // Nothing to do here. |native_pixmap| will be freed when this function
  // returns and reference count drops to 0.
}

}  // namespace

MappableBufferNativePixmap::MappableBufferNativePixmap(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    std::unique_ptr<gfx::ClientNativePixmap> pixmap)
    : size_(size), format_(format), pixmap_(std::move(pixmap)) {}

MappableBufferNativePixmap::~MappableBufferNativePixmap() {
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(map_lock_);
    DCHECK_EQ(map_count_, 0u);
  }
#endif
}

void MappableBufferNativePixmap::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

// static
std::unique_ptr<MappableBufferNativePixmap>
MappableBufferNativePixmap::CreateFromHandle(
    gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  CHECK(viz::HasEquivalentBufferFormat(format));
  std::unique_ptr<gfx::ClientNativePixmap> native_pixmap =
      client_native_pixmap_factory->ImportFromHandle(
          std::move(handle).native_pixmap_handle(), size, format, usage);
  if (!native_pixmap) {
    return nullptr;
  }

  return base::WrapUnique(
      new MappableBufferNativePixmap(size, format, std::move(native_pixmap)));
}

// static
base::OnceClosure MappableBufferNativePixmap::AllocateForTesting(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  scoped_refptr<gfx::NativePixmap> pixmap;
  pixmap = ui::OzonePlatform::GetInstance()
               ->GetSurfaceFactoryOzone()
               ->CreateNativePixmap(gfx::kNullAcceleratedWidget, nullptr, size,
                                    format, usage);
  if (!pixmap) {
    // https://crrev.com/c/5348599
    // In some format + usage combination the pixmap may be null. For example,
    // YUV_420_BIPLANAR + SCANOUT_CAMERA_READ_WRITE may fail to allocate because
    // only some of platform supports that.
    LOG(WARNING) << "Failed to allocate pixmap " << format.ToString() << " + "
                 << gfx::BufferUsageToString(usage);
  } else {
    *handle = gfx::GpuMemoryBufferHandle(pixmap->ExportHandle());
  }
  // It's safe to bind FreeNativePixmapForTesting even if pixmap is not created
  // as it does nothing with the pixmap. See the comment in
  // FreeNativePixmapForTesting for more details.
  return base::BindOnce(&FreeNativePixmapForTesting, pixmap);
}

bool MappableBufferNativePixmap::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++) {
    return true;
  }

  if (format_.NumberOfPlanes() !=
      static_cast<int>(pixmap_->GetNumberOfPlanes())) {
    // RGBX8888 and BGR_565 allocates 2 planes while the gfx function returns 1
    LOG(WARNING) << "Mismatched plane count " << format_.ToString()
                 << " expected " << format_.NumberOfPlanes() << " value "
                 << pixmap_->GetNumberOfPlanes();
  }

  if (!pixmap_->Map()) {
    --map_count_;
    return false;
  }

  return true;
}

void* MappableBufferNativePixmap::memory(size_t plane) {
  AssertMapped();
  return pixmap_->GetMemoryAddress(plane);
}

void MappableBufferNativePixmap::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  if (--map_count_) {
    return;
  }

  pixmap_->Unmap();
}

int MappableBufferNativePixmap::stride(size_t plane) const {
  // The caller is responsible for ensuring that |plane| is within bounds.
  CHECK_LT(plane, pixmap_->GetNumberOfPlanes());
  return pixmap_->GetStride(plane);
}

gfx::GpuMemoryBufferType MappableBufferNativePixmap::GetType() const {
  return gfx::NATIVE_PIXMAP;
}

gfx::GpuMemoryBufferHandle MappableBufferNativePixmap::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle(pixmap_->CloneHandleForIPC());
  return handle;
}

void MappableBufferNativePixmap::MapAsync(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(Map());
}

bool MappableBufferNativePixmap::AsyncMappingIsNonBlocking() const {
  return false;
}

}  // namespace gpu
