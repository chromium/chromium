// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
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

GpuMemoryBufferImplNativePixmap::GpuMemoryBufferImplNativePixmap(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    DestructionCallback callback,
    std::unique_ptr<gfx::ClientNativePixmap> pixmap)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      pixmap_(std::move(pixmap)) {}

GpuMemoryBufferImplNativePixmap::~GpuMemoryBufferImplNativePixmap() = default;

// static
std::unique_ptr<GpuMemoryBufferImplNativePixmap>
GpuMemoryBufferImplNativePixmap::CreateFromHandle(
    gfx::ClientNativePixmapFactory* client_native_pixmap_factory,
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    DestructionCallback callback) {
  std::unique_ptr<gfx::ClientNativePixmap> native_pixmap =
      client_native_pixmap_factory->ImportFromHandle(
          std::move(handle.native_pixmap_handle), size, format, usage);
  if (!native_pixmap)
    return nullptr;

  return base::WrapUnique(new GpuMemoryBufferImplNativePixmap(
      handle.id, size, format, std::move(callback), std::move(native_pixmap)));
}

// static
base::OnceClosure GpuMemoryBufferImplNativePixmap::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
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
    LOG(WARNING) << "Failed to allocate pixmap "
                 << gfx::BufferFormatToString(format) << " + "
                 << gfx::BufferUsageToString(usage);
  } else {
    handle->native_pixmap_handle = pixmap->ExportHandle();
    handle->type = gfx::NATIVE_PIXMAP;
  }
  // It's safe to bind FreeNativePixmapForTesting even if pixmap is not created
  // as it does nothing with the pixmap. See the comment in
  // FreeNativePixmapForTesting for more details.
  return base::BindOnce(&FreeNativePixmapForTesting, pixmap);
}

bool GpuMemoryBufferImplNativePixmap::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++)
    return true;

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

void* GpuMemoryBufferImplNativePixmap::memory(size_t plane) {
  AssertMapped();
  return pixmap_->GetMemoryAddress(plane);
}

void GpuMemoryBufferImplNativePixmap::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  if (--map_count_)
    return;

  pixmap_->Unmap();
}

int GpuMemoryBufferImplNativePixmap::stride(size_t plane) const {
  // The caller is responsible for ensuring that |plane| is within bounds.
  CHECK_LT(plane, pixmap_->GetNumberOfPlanes());
  return pixmap_->GetStride(plane);
}

gfx::GpuMemoryBufferType GpuMemoryBufferImplNativePixmap::GetType() const {
  return gfx::NATIVE_PIXMAP;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplNativePixmap::CloneHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.id = id_;
  handle.native_pixmap_handle = pixmap_->CloneHandleForIPC();
  return handle;
}

}  // namespace gpu
