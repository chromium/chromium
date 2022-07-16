// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_shared_memory.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
namespace {
size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

SkImageInfo MakeSkImageInfo(const gfx::Size& size, viz::ResourceFormat format) {
  return SkImageInfo::Make(size.width(), size.height(),
                           ResourceFormatToClosestSkColorType(
                               /*gpu_compositing=*/true, format),
                           kOpaque_SkAlphaType);
}

class SharedImageRepresentationMemorySharedMemory
    : public SharedImageRepresentationMemory {
 public:
  SharedImageRepresentationMemorySharedMemory(SharedImageManager* manager,
                                              SharedImageBacking* backing,
                                              MemoryTypeTracker* tracker)
      : SharedImageRepresentationMemory(manager, backing, tracker) {}

 protected:
  SkPixmap BeginReadAccess() override {
    SkImageInfo info = MakeSkImageInfo(shared_image_shared_memory()->size(),
                                       shared_image_shared_memory()->format());
    return SkPixmap(
        info, shared_image_shared_memory()->shared_memory_wrapper().GetMemory(),
        shared_image_shared_memory()->shared_memory_wrapper().GetStride());
  }

 private:
  SharedImageBackingSharedMemory* shared_image_shared_memory() {
    return static_cast<SharedImageBackingSharedMemory*>(backing());
  }
};

}  // namespace

SharedImageBackingSharedMemory::~SharedImageBackingSharedMemory() = default;

void SharedImageBackingSharedMemory::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  // Intentionally no-op for now. Will be called by clients later
}

const SharedMemoryRegionWrapper&
SharedImageBackingSharedMemory::shared_memory_wrapper() {
  return shared_memory_wrapper_;
}

bool SharedImageBackingSharedMemory::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  NOTREACHED();
  return false;
}

gfx::Rect SharedImageBackingSharedMemory::ClearedRect() const {
  NOTREACHED();
  return gfx::Rect();
}

void SharedImageBackingSharedMemory::SetClearedRect(
    const gfx::Rect& cleared_rect) {
  NOTREACHED();
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingSharedMemory::ProduceDawn(SharedImageManager* manager,
                                            MemoryTypeTracker* tracker,
                                            WGPUDevice device,
                                            WGPUBackendType backend_type) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingSharedMemory::ProduceGLTexture(SharedImageManager* manager,
                                                 MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingSharedMemory::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingSharedMemory::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingSharedMemory::ProduceOverlay(SharedImageManager* manager,
                                               MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationVaapi>
SharedImageBackingSharedMemory::ProduceVASurface(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VaapiDependenciesFactory* dep_factory) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationMemory>
SharedImageBackingSharedMemory::ProduceMemory(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  if (!shared_memory_wrapper_.IsValid())
    return nullptr;

  return std::make_unique<SharedImageRepresentationMemorySharedMemory>(
      manager, this, tracker);
}

SharedImageBackingSharedMemory::SharedImageBackingSharedMemory(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    SharedMemoryRegionWrapper wrapper)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         EstimatedSize(format, size),
                         false),
      shared_memory_wrapper_(std::move(wrapper)) {}
}  // namespace gpu
