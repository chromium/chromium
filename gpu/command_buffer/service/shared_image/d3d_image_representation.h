// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/buildflags.h"

namespace gpu {

// Representation of a D3DImageBacking as a GL TexturePassthrough.
class GLTexturePassthroughD3DImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughD3DImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      std::vector<scoped_refptr<D3DImageBacking::GLTextureHolder>>
          texture_holders);
  ~GLTexturePassthroughD3DImageRepresentation() override;

  bool NeedsSuspendAccessForDXGIKeyedMutex() const override;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;

  void* GetEGLImage();

 private:
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  // Holds a gles2::TexturePassthrough and corresponding egl image.
  std::vector<scoped_refptr<D3DImageBacking::GLTextureHolder>>
      gl_texture_holders_;
};

// Representation of a D3DImageBacking as a Dawn Texture
#if BUILDFLAG(USE_DAWN)
class DawnD3DImageRepresentation : public DawnImageRepresentation {
 public:
  DawnD3DImageRepresentation(SharedImageManager* manager,
                             SharedImageBacking* backing,
                             MemoryTypeTracker* tracker,
                             const wgpu::Device& device,
                             wgpu::BackendType backend_type);
  ~DawnD3DImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage) override;
  void EndAccess() override;

 private:
  const wgpu::Device device_;
  const wgpu::BackendType backend_type_;
  wgpu::Texture texture_;
};
#endif  // BUILDFLAG(USE_DAWN)

// Representation of a D3DImageBacking as an overlay.
class OverlayD3DImageRepresentation : public OverlayImageRepresentation {
 public:
  OverlayD3DImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);
  ~OverlayD3DImageRepresentation() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;

  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() override;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
};

class D3D11VideoDecodeImageRepresentation
    : public VideoDecodeImageRepresentation {
 public:
  D3D11VideoDecodeImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture);
  ~D3D11VideoDecodeImageRepresentation() override;

 private:
  bool BeginWriteAccess() override;
  void EndWriteAccess() override;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const override;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;
};

}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_
