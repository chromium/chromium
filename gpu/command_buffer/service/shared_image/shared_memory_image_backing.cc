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
#include "ui/gfx/geometry/skia_conversions.h"

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
    DCHECK(format().is_single_plane());
    return shared_image_shared_memory()->pixmaps()[0];
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
                                 MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}

  ~OverlayImageRepresentationImpl() override = default;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {}

#if BUILDFLAG(IS_WIN)
  absl::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() override {
    // This should only be called for the backing which references the Y plane,
    // eg. plane_index=0, of an NV12 shmem GMB - see allow_shm_overlay in
    // SharedImageFactory. This allows access to both Y and UV planes.
    const auto& shm_wrapper = static_cast<SharedMemoryImageBacking*>(backing())
                                  ->shared_memory_wrapper();
    return absl::make_optional<gl::DCLayerOverlayImage>(
        size(), shm_wrapper.GetMemory(0), shm_wrapper.GetStride(0));
  }
#endif
};

}  // namespace

SharedMemoryImageBacking::~SharedMemoryImageBacking() = default;

void SharedMemoryImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  // Intentionally no-op for now. Will be called by clients later
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

const SharedMemoryRegionWrapper&
SharedMemoryImageBacking::shared_memory_wrapper() {
  return shared_memory_wrapper_;
}

const std::vector<SkPixmap>& SharedMemoryImageBacking::pixmaps() {
  return pixmaps_;
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
  return std::make_unique<OverlayImageRepresentationImpl>(manager, this,
                                                          tracker);
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

base::trace_event::MemoryAllocatorDump* SharedMemoryImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  auto* dump = SharedImageBacking::OnMemoryDump(dump_name, client_guid, pmd,
                                                client_tracing_id);

  // Add a |shared_memory_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto shared_memory_guid = shared_memory_wrapper_.GetMappingGuid();
  if (!shared_memory_guid.is_empty()) {
    pmd->CreateSharedMemoryOwnershipEdge(
        client_guid, shared_memory_guid,
        static_cast<int>(TracingImportance::kNotOwner));
  }
  return dump;
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
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         format.EstimatedSizeInBytes(size),
                         false),
      shared_memory_wrapper_(std::move(wrapper)) {
  DCHECK(shared_memory_wrapper_.IsValid());

  for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    auto info = SkImageInfo::Make(gfx::SizeToSkISize(plane_size),
                                  viz::ToClosestSkColorType(
                                      /*gpu_compositing=*/true, format, plane),
                                  alpha_type, color_space.ToSkColorSpace());
    pixmaps_.push_back(shared_memory_wrapper_.MakePixmapForPlane(info, plane));
  }
}

}  // namespace gpu
