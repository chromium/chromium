// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_representation.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

DXGISwapChainOverlayImageRepresentation::
    DXGISwapChainOverlayImageRepresentation(SharedImageManager* manager,
                                            SharedImageBacking* backing,
                                            MemoryTypeTracker* tracker)
    : OverlayImageRepresentation(manager, backing, tracker) {}

DXGISwapChainOverlayImageRepresentation::
    ~DXGISwapChainOverlayImageRepresentation() = default;

std::optional<gl::DCLayerOverlayImage>
DXGISwapChainOverlayImageRepresentation::GetDCLayerOverlayImage() {
  return static_cast<DXGISwapChainImageBacking*>(backing())
      ->GetDCLayerOverlayImage();
}

bool DXGISwapChainOverlayImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  // For the time being, let's use present interval 0.
  const bool should_synchronize_present_with_vblank = false;

  bool success = static_cast<DXGISwapChainImageBacking*>(backing())->Present(
      should_synchronize_present_with_vblank);

  return success;
}

void DXGISwapChainOverlayImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {}

GLTexturePassthroughDXGISwapChainBufferRepresentation::
    GLTexturePassthroughDXGISwapChainBufferRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<D3DImageBacking::GLTextureHolder> texture_holder)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      texture_holder_(std::move(texture_holder)) {}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughDXGISwapChainBufferRepresentation::GetTexturePassthrough(
    int plane_index) {
  DCHECK_EQ(plane_index, 0);
  return texture_holder_->texture_passthrough();
}

GLTexturePassthroughDXGISwapChainBufferRepresentation::
    ~GLTexturePassthroughDXGISwapChainBufferRepresentation() = default;

bool GLTexturePassthroughDXGISwapChainBufferRepresentation::BeginAccess(
    GLenum mode) {
  return true;
}

void GLTexturePassthroughDXGISwapChainBufferRepresentation::EndAccess() {}

// static
std::unique_ptr<SkiaGLImageRepresentationDXGISwapChain>
SkiaGLImageRepresentationDXGISwapChain::Create(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker) {
  GrBackendTexture backend_texture;
  GLFormatDesc format_desc = context_state->GetGLFormatCaps().ToGLFormatDesc(
      backing->format(), /*plane_index=*/0);
  if (!GetGrBackendTexture(
          context_state->feature_info(),
          gl_representation->GetTextureBase()->target(), backing->size(),
          gl_representation->GetTextureBase()->service_id(),
          format_desc.storage_internal_format,
          context_state->gr_context()->threadSafeProxy(), &backend_texture)) {
    return nullptr;
  }
  auto promise_texture = GrPromiseImageTexture::Make(backend_texture);
  if (!promise_texture)
    return nullptr;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures = {
      promise_texture};
  return base::WrapUnique(new SkiaGLImageRepresentationDXGISwapChain(
      std::move(gl_representation), std::move(promise_textures),
      std::move(context_state), manager, backing, tracker));
}

SkiaGLImageRepresentationDXGISwapChain::SkiaGLImageRepresentationDXGISwapChain(
    std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
    scoped_refptr<SharedContextState> context_state,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaGLImageRepresentation(std::move(gl_representation),
                                std::move(promise_textures),
                                std::move(context_state),
                                manager,
                                backing,
                                tracker) {}

SkiaGLImageRepresentationDXGISwapChain::
    ~SkiaGLImageRepresentationDXGISwapChain() = default;

std::vector<sk_sp<SkSurface>>
SkiaGLImageRepresentationDXGISwapChain::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<skgpu::MutableTextureState>* end_state) {
  std::vector<sk_sp<SkSurface>> surfaces =
      SkiaGLImageRepresentation::BeginWriteAccess(
          final_msaa_count, surface_props, update_rect, begin_semaphores,
          end_semaphores, end_state);

  if (!surfaces.empty()) {
    if (!static_cast<DXGISwapChainImageBacking*>(backing())
             ->DidBeginWriteAccess(update_rect)) {
      return {};
    }
  }

  return surfaces;
}

void SkiaGLImageRepresentationDXGISwapChain::EndWriteAccess() {
  SkiaGLImageRepresentation::EndWriteAccess();

  // For FLIP_SEQUENTIAL swap chains, a successful present will unbind the back
  // buffer from the graphics pipeline. The state caching layers in Skia/ANGLE
  // are unaware of our Present1 calls and incorrectly assume the back buffer
  // texture remains bound. To work around this, we'll recreate the SkSurfaces
  // for every draw to ensure that Skia/ANGLE will rebind out back buffer.
  // See:
  // https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgiswapchain1-present1#remarks
  //
  // This only needs to happen on Present, but we have it on EndWriteAccess for
  // convenience. It's possible to have multiple draws per Present, but we
  // assume that 1:1 is the most common case.
  SkiaGLImageRepresentation::ClearCachedSurfaces();
}

DawnRepresentationDXGISwapChain::DawnRepresentationDXGISwapChain(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    wgpu::Device device,
    wgpu::BackendType backend_type)
    : DawnImageRepresentation(manager, backing, tracker), device_(device) {
  DCHECK(device_);
}

DawnRepresentationDXGISwapChain::~DawnRepresentationDXGISwapChain() {
  EndAccess();
}

wgpu::Texture DawnRepresentationDXGISwapChain::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const gfx::Rect& update_rect) {
  auto* swapchain_backing = static_cast<DXGISwapChainImageBacking*>(backing());
  texture_ = swapchain_backing->BeginAccessDawn(device_, usage, internal_usage,
                                                update_rect);
  return texture_;
}

wgpu::Texture DawnRepresentationDXGISwapChain::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage) {
  NOTREACHED();
}

void DawnRepresentationDXGISwapChain::EndAccess() {
  if (!texture_) {
    return;
  }

  auto* swapchain_backing = static_cast<DXGISwapChainImageBacking*>(backing());
  swapchain_backing->EndAccessDawn(device_, std::move(texture_));
}

}  // namespace gpu
