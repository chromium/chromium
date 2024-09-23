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

std::optional<gl::DCLayerOverlayImage>
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

DCompSurfaceSkiaGaneshImageRepresentation::
    DCompSurfaceSkiaGaneshImageRepresentation(
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

DCompSurfaceSkiaGaneshImageRepresentation::
    ~DCompSurfaceSkiaGaneshImageRepresentation() = default;

std::vector<sk_sp<SkSurface>>
DCompSurfaceSkiaGaneshImageRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  DCompSurfaceImageBacking* dcomp_backing =
      static_cast<DCompSurfaceImageBacking*>(backing());
  sk_sp<SkSurface> surface = dcomp_backing->BeginDrawGanesh(
      context_state_.get(), final_msaa_count, surface_props, update_rect);
  if (!surface) {
    return {};
  }

  return {std::move(surface)};
}

void DCompSurfaceSkiaGaneshImageRepresentation::EndWriteAccess() {
  DCompSurfaceImageBacking* dcomp_backing =
      static_cast<DCompSurfaceImageBacking*>(backing());
  dcomp_backing->EndDrawGanesh();
}

std::vector<sk_sp<GrPromiseImageTexture>>
DCompSurfaceSkiaGaneshImageRepresentation::BeginWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  NOTREACHED_IN_MIGRATION();
  return {};
}

std::vector<sk_sp<GrPromiseImageTexture>>
DCompSurfaceSkiaGaneshImageRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  NOTREACHED_IN_MIGRATION();
  return {};
}

void DCompSurfaceSkiaGaneshImageRepresentation::EndReadAccess() {
  NOTREACHED_IN_MIGRATION();
}

DCompSurfaceDawnImageRepresentation::DCompSurfaceDawnImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type)
    : DawnImageRepresentation(manager, backing, tracker), device_(device) {}

DCompSurfaceDawnImageRepresentation::~DCompSurfaceDawnImageRepresentation() {
  EndAccess();
}

wgpu::Texture DCompSurfaceDawnImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const gfx::Rect& update_rect) {
  DCompSurfaceImageBacking* dcomp_backing =
      static_cast<DCompSurfaceImageBacking*>(backing());
  texture_ =
      dcomp_backing->BeginDrawDawn(device_, usage, internal_usage, update_rect);
  return texture_;
}

wgpu::Texture DCompSurfaceDawnImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage) {
  NOTREACHED();
}

void DCompSurfaceDawnImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  // Do this before further operations since those could end up destroying the
  // Dawn device and we want the fence to be duplicated before then.
  DCompSurfaceImageBacking* dcomp_backing =
      static_cast<DCompSurfaceImageBacking*>(backing());
  dcomp_backing->EndDrawDawn(device_, std::move(texture_));
}

}  // namespace gpu
