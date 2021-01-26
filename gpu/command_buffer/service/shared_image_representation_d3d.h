// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_D3D_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_D3D_H_

#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/buildflags.h"

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn_native/D3D12Backend.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

class SharedImageBackingD3D;

// Representation of a SharedImageBackingD3D as a GL TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughD3D
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughD3D(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture);
  ~SharedImageRepresentationGLTexturePassthroughD3D() override;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;

 private:
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  scoped_refptr<gles2::TexturePassthrough> texture_;
};

// Representation of a SharedImageBackingD3D as a Dawn Texture
#if BUILDFLAG(USE_DAWN)
class SharedImageRepresentationDawnD3D : public SharedImageRepresentationDawn {
 public:
  SharedImageRepresentationDawnD3D(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker,
                                   WGPUDevice device);

  ~SharedImageRepresentationDawnD3D() override;

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  WGPUDevice device_;
  WGPUTexture texture_ = nullptr;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  DawnProcTable dawn_procs_;
};
#endif  // BUILDFLAG(USE_DAWN)

// Representation of a SharedImageBackingD3D as an overlay.
class SharedImageRepresentationOverlayD3D
    : public SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayD3D(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker);
  ~SharedImageRepresentationOverlayD3D() override = default;

 private:
  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;

  gl::GLImage* GetGLImage() override;
};

}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_D3D_H_
