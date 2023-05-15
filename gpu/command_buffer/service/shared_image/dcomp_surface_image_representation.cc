// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_representation.h"

#include "base/win/windows_types.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gpu {

DCompSurfaceOverlayImageRepresentation::DCompSurfaceOverlayImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : OverlayImageRepresentation(manager, backing, tracker) {}

DCompSurfaceOverlayImageRepresentation::
    ~DCompSurfaceOverlayImageRepresentation() = default;

absl::optional<gl::DCLayerOverlayImage>
DCompSurfaceOverlayImageRepresentation::GetDCLayerOverlayImage() {
  return static_cast<DCompSurfaceImageBacking*>(backing())
      ->GetDCLayerOverlayImage();
}

bool DCompSurfaceOverlayImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  // DComp surfaces access synchronization happens internally in DWM on commit.
  return true;
}

void DCompSurfaceOverlayImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {}

DCompSurfaceSkiaImageRepresentation::DCompSurfaceSkiaImageRepresentation(
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                    manager,
                                    backing,
                                    tracker),
      context_state_(std::move(context_state)) {
  DCHECK(context_state_);
}

DCompSurfaceSkiaImageRepresentation::~DCompSurfaceSkiaImageRepresentation() =
    default;

std::vector<sk_sp<SkSurface>>
DCompSurfaceSkiaImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  sk_sp<SkSurface> surface =
      static_cast<DCompSurfaceImageBacking*>(backing())->BeginDraw(
          context_state_.get(), final_msaa_count, surface_props, update_rect);
  if (!surface) {
    return {};
  }

  return {std::move(surface)};
}

void DCompSurfaceSkiaImageRepresentation::EndWriteAccess() {
  bool success = static_cast<DCompSurfaceImageBacking*>(backing())->EndDraw();
  DCHECK(success);
}

std::vector<sk_sp<SkPromiseImageTexture>>
DCompSurfaceSkiaImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  NOTREACHED();
  return {};
}

std::vector<sk_sp<SkPromiseImageTexture>>
DCompSurfaceSkiaImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  NOTREACHED();
  return {};
}

void DCompSurfaceSkiaImageRepresentation::EndReadAccess() {
  NOTREACHED();
}

}  // namespace gpu
