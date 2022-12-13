// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/buildflags.h"

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn/native/D3D12Backend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

class D3DImageBacking;

// Representation of a D3DImageBacking as a GL TexturePassthrough.
class GLTexturePassthroughD3DImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughD3DImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture);
  ~GLTexturePassthroughD3DImageRepresentation() override;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;

 private:
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  scoped_refptr<gles2::TexturePassthrough> texture_;
};

// Representation of a D3DImageBacking as a Dawn Texture
#if BUILDFLAG(USE_DAWN)
class DawnD3DImageRepresentation : public DawnImageRepresentation {
 public:
  DawnD3DImageRepresentation(SharedImageManager* manager,
                             SharedImageBacking* backing,
                             MemoryTypeTracker* tracker,
                             WGPUDevice device);
  ~DawnD3DImageRepresentation() override;

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  const WGPUDevice device_;
  WGPUTexture texture_ = nullptr;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  DawnProcTable dawn_procs_;
};
#endif  // BUILDFLAG(USE_DAWN)

// Representation of a D3DImageBacking as an overlay.
class OverlayD3DImageRepresentation : public OverlayImageRepresentation {
 public:
  OverlayD3DImageRepresentation(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker,
                                scoped_refptr<gl::GLImage> gl_image);
  ~OverlayD3DImageRepresentation() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;

  gl::GLImage* GetGLImage() override;

  scoped_refptr<gl::GLImage> gl_image_;
};

class D3D11VideoDecodeImageRepresentation
    : public VideoDecodeImageRepresentation {
 public:
  D3D11VideoDecodeImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture);
  ~D3D11VideoDecodeImageRepresentation() override;

 private:
  bool BeginWriteAccess() override;
  void EndWriteAccess() override;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const override;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
};

}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_REPRESENTATION_H_
