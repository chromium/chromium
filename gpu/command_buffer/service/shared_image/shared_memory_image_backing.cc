// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image_memory.h"

namespace gpu {
namespace {

class MemoryImageRepresentationImpl : public MemoryImageRepresentation {
 public:
  MemoryImageRepresentationImpl(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : MemoryImageRepresentation(manager, backing, tracker) {}

 protected:
  SkPixmap BeginReadAccess() override {
    return SkPixmap(
        backing()->AsSkImageInfo(),
        shared_image_shared_memory()->shared_memory_wrapper().GetMemory(),
        shared_image_shared_memory()->shared_memory_wrapper().GetStride());
  }

 private:
  SharedMemoryImageBacking* shared_image_shared_memory() {
    return static_cast<SharedMemoryImageBacking*>(backing());
  }
};

class OverlayImageRepresentationImpl : public OverlayImageRepresentation {
 public:
  OverlayImageRepresentationImpl(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker,
                                 scoped_refptr<gl::GLImage> gl_image)
      : OverlayImageRepresentation(manager, backing, tracker),
        gl_image_(std::move(gl_image)) {}

  ~OverlayImageRepresentationImpl() override = default;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {}

#if BUILDFLAG(IS_WIN)
  gl::GLImage* GetGLImage() override { return gl_image_.get(); }
#endif

  scoped_refptr<gl::GLImage> gl_image_;
};

}  // namespace

SharedMemoryImageBacking::~SharedMemoryImageBacking() = default;

void SharedMemoryImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  // Intentionally no-op for now. Will be called by clients later
}

const SharedMemoryRegionWrapper&
SharedMemoryImageBacking::shared_memory_wrapper() {
  return shared_memory_wrapper_;
}

SharedImageBackingType SharedMemoryImageBacking::GetType() const {
  return SharedImageBackingType::kSharedMemory;
}

gfx::Rect SharedMemoryImageBacking::ClearedRect() const {
  NOTREACHED();
  return gfx::Rect();
}

void SharedMemoryImageBacking::SetClearedRect(const gfx::Rect& cleared_rect) {
  NOTREACHED();
}

std::unique_ptr<DawnImageRepresentation> SharedMemoryImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<GLTextureImageRepresentation>
SharedMemoryImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                           MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
SharedMemoryImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SkiaImageRepresentation> SharedMemoryImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<OverlayImageRepresentation>
SharedMemoryImageBacking::ProduceOverlay(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  if (!shared_memory_wrapper_.IsValid())
    return nullptr;

  auto gl_image =
      base::WrapRefCounted<gl::GLImageMemory>(new gl::GLImageMemory(size()));
  if (!gl_image->Initialize(shared_memory_wrapper_.GetMemory(),
                            ToBufferFormat(format()),
                            shared_memory_wrapper_.GetStride(),
                            /*disable_pbo_upload=*/true)) {
    DLOG(ERROR) << "Failed to initialize GLImageMemory";
    return nullptr;
  }
  return std::make_unique<OverlayImageRepresentationImpl>(
      manager, this, tracker, std::move(gl_image));
}

std::unique_ptr<VaapiImageRepresentation>
SharedMemoryImageBacking::ProduceVASurface(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VaapiDependenciesFactory* dep_factory) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<MemoryImageRepresentation>
SharedMemoryImageBacking::ProduceMemory(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  if (!shared_memory_wrapper_.IsValid())
    return nullptr;

  return std::make_unique<MemoryImageRepresentationImpl>(manager, this,
                                                         tracker);
}

void SharedMemoryImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                   client_tracing_id);

  // Add a |shared_memory_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto shared_memory_guid = shared_memory_wrapper_.GetMappingGuid();
  if (!shared_memory_guid.is_empty()) {
    pmd->CreateSharedMemoryOwnershipEdge(client_guid, shared_memory_guid,
                                         kNonOwningEdgeImportance);
  }
}

SharedMemoryImageBacking::SharedMemoryImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    SharedMemoryRegionWrapper wrapper)
    : SharedImageBacking(
          mailbox,
          format,
          size,
          color_space,
          surface_origin,
          alpha_type,
          usage,
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format),
          false),
      shared_memory_wrapper_(std::move(wrapper)) {}
}  // namespace gpu
