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
#include "ui/base/ui_base_features.h"
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
  scoped_refptr<gfx::NativePixmap> pixmap;
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    pixmap = ui::OzonePlatform::GetInstance()
                 ->GetSurfaceFactoryOzone()
                 ->CreateNativePixmap(gfx::kNullAcceleratedWidget,
                                      VK_NULL_HANDLE, size, format, usage);
    handle->native_pixmap_handle = pixmap->ExportHandle();
  } else
#endif
  {
    // TODO(j.isorce): use gbm_bo_create / gbm_bo_get_fd from system libgbm.
    NOTIMPLEMENTED();
  }
  handle->type = gfx::NATIVE_PIXMAP;
  return base::BindOnce(&FreeNativePixmapForTesting, pixmap);
}

bool GpuMemoryBufferImplNativePixmap::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++)
    return true;

  DCHECK_EQ(gfx::NumberOfPlanesForLinearBufferFormat(GetFormat()),
            handle_.planes.size());
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
  CHECK_LT(plane, handle_.planes.size());

  // |handle_|.planes[plane].stride is a uint32_t. For usages for which we
  // create a ClientNativePixmapDmaBuf,
  // ClientNativePixmapDmaBuf::ImportFromDmabuf() ensures that the stride fits
  // on an int, so this checked_cast shouldn't fail. For usages for which we
  // create a ClientNativePixmapOpaque, we don't validate the stride, but the
  // expectation is that either a) the stride() method won't be called, or b)
  // the stride() method is called on the GPU process and
  // |handle_|.planes[plane].stride is also set on the GPU process so there's no
  // need to validate it. Refer to http://crbug.com/1093644#c1 for a more
  // detailed discussion.
  return base::checked_cast<int>(handle_.planes[plane].stride);
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
