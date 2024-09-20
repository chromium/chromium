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
class DawnD3DImageRepresentation : public DawnImageRepresentation {
 public:
  DawnD3DImageRepresentation(SharedImageManager* manager,
                             SharedImageBacking* backing,
                             MemoryTypeTracker* tracker,
                             const wgpu::Device& device,
                             wgpu::BackendType backend_type,
                             std::vector<wgpu::TextureFormat> view_formats);
  ~DawnD3DImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  void EndAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

 private:
  const wgpu::Device device_;
  const wgpu::BackendType backend_type_;
  wgpu::Texture texture_;
  std::vector<wgpu::TextureFormat> view_formats_;
};

class DawnD3DBufferRepresentation : public DawnBufferRepresentation {
 public:
  DawnD3DBufferRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              const wgpu::Device& device,
                              wgpu::BackendType backend_type);
  ~DawnD3DBufferRepresentation() override;

  wgpu::Buffer BeginAccess(wgpu::BufferUsage usage) override;
  void EndAccess() override;

 private:
  const wgpu::Device device_;
  const wgpu::BackendType backend_type_;
  wgpu::Buffer buffer_;
};

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

class D3D11VideoImageRepresentation : public VideoImageRepresentation {
 public:
  D3D11VideoImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture);
  ~D3D11VideoImageRepresentation() override;

 private:
  bool BeginWriteAccess() override;
  void EndWriteAccess() override;
  bool BeginReadAccess() override;
  void EndReadAccess() override;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const override;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;
};

}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_
