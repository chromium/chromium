// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"

#include <vulkan/vulkan.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

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
    std::unique_ptr<gfx::ClientNativePixmap> pixmap,
    gfx::NativePixmapHandle handle)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      pixmap_(std::move(pixmap)),
      handle_(std::move(handle)) {}

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
          CloneHandleForIPC(handle.native_pixmap_handle), size, format, usage);
  if (!native_pixmap)
    return nullptr;

  return base::WrapUnique(new GpuMemoryBufferImplNativePixmap(
      handle.id, size, format, std::move(callback), std::move(native_pixmap),
      std::move(handle.native_pixmap_handle)));
}

// static
base::OnceClosure GpuMemoryBufferImplNativePixmap::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
#if defined(USE_OZONE)
  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gfx::kNullAcceleratedWidget, VK_NULL_HANDLE,
                               size, format, usage);
  handle->native_pixmap_handle = pixmap->ExportHandle();
#else
  // TODO(j.isorce): use gbm_bo_create / gbm_bo_get_fd from system libgbm.
  scoped_refptr<gfx::NativePixmap> pixmap;
  NOTIMPLEMENTED();
#endif
  handle->type = gfx::NATIVE_PIXMAP;
  return base::BindOnce(&FreeNativePixmapForTesting, pixmap);
}

bool GpuMemoryBufferImplNativePixmap::Map() {
  DCHECK(!mapped_);
  DCHECK_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetFormat()),
            handle_.planes.size());
  mapped_ = pixmap_->Map();
  return mapped_;
}

void* GpuMemoryBufferImplNativePixmap::memory(size_t plane) {
  DCHECK(mapped_);
  return pixmap_->GetMemoryAddress(plane);
}

void GpuMemoryBufferImplNativePixmap::Unmap() {
  DCHECK(mapped_);
  pixmap_->Unmap();
  mapped_ = false;
}

int GpuMemoryBufferImplNativePixmap::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
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
  handle.native_pixmap_handle = gfx::CloneHandleForIPC(handle_);
  return handle;
}

}  // namespace gpu
