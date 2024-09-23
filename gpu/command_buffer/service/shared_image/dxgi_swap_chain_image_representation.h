// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"

class GrPromiseImageTexture;

namespace gpu {

// See DXGISwapChainImageBacking::ProduceOverlay for more information.
class DXGISwapChainOverlayImageRepresentation
    : public OverlayImageRepresentation {
 public:
  DXGISwapChainOverlayImageRepresentation(SharedImageManager* manager,
                                          SharedImageBacking* backing,
                                          MemoryTypeTracker* tracker);
  ~DXGISwapChainOverlayImageRepresentation() override;

 protected:
  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
};

// Representation of a DXGI swap chain backbuffer as a GL TexturePassthrough.
class GLTexturePassthroughDXGISwapChainBufferRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughDXGISwapChainBufferRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<D3DImageBacking::GLTextureHolder> texture_holder);
  ~GLTexturePassthroughDXGISwapChainBufferRepresentation() override;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;

 private:
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  // Holds a gles2::TexturePassthrough and corresponding egl image.
  scoped_refptr<D3DImageBacking::GLTextureHolder> texture_holder_;
};

// Functionally the same as SkiaGLImageRepresentation, with the exception of
// intercepting the update rect passed to BeginWriteAccess to notify the
// DXGISwapChainImageBacking for IDXGISwapChain1::Present1.
class SkiaGLImageRepresentationDXGISwapChain
    : public SkiaGLImageRepresentation {
 public:
  static std::unique_ptr<SkiaGLImageRepresentationDXGISwapChain> Create(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);

  ~SkiaGLImageRepresentationDXGISwapChain() override;

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;

  void EndWriteAccess() override;

 private:
  SkiaGLImageRepresentationDXGISwapChain(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);
};

// Representation of a DXGI swap chain backbuffer as a Dawn texture.
class DawnRepresentationDXGISwapChain : public DawnImageRepresentation {
 public:
  DawnRepresentationDXGISwapChain(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker,
                                  wgpu::Device device,
                                  wgpu::BackendType backend_type);
  ~DawnRepresentationDXGISwapChain() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage,
                            const gfx::Rect& update_rect) override;
  void EndAccess() override;

 private:
  const wgpu::Device device_;
  wgpu::Texture texture_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_REPRESENTATION_H_
