// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_representation.h"

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

GLTexturePassthroughD3DImageRepresentation::
    GLTexturePassthroughD3DImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        std::vector<scoped_refptr<gles2::TexturePassthrough>> textures)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      textures_(std::move(textures)) {}

GLTexturePassthroughD3DImageRepresentation::
    ~GLTexturePassthroughD3DImageRepresentation() = default;

bool GLTexturePassthroughD3DImageRepresentation::
    NeedsSuspendAccessForDXGIKeyedMutex() const {
  return static_cast<D3DImageBacking*>(backing())->has_keyed_mutex();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughD3DImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  return textures_[plane_index];
}

bool GLTexturePassthroughD3DImageRepresentation::BeginAccess(GLenum mode) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  for (int plane = 0; plane < format().NumberOfPlanes(); plane++) {
    // Bind the GLImage if necessary.
    auto texture = GetTexturePassthrough(plane);
    if (texture->is_bind_pending()) {
      GLenum target = texture->target();
      gl::GLImage* image = texture->GetLevelImage(target, 0);

      if (image) {
        // First ensure that |target| is bound to |texture|.
        gl::GLApi* const api = gl::g_current_gl_context;
        gl::ScopedRestoreTexture scoped_restore(api, target);
        api->glBindTextureFn(target, texture->service_id());

        auto* image_d3d = gl::GLImage::ToGLImageD3D(image);
        if (image_d3d) {
          // Bind the GLImage to |texture| via |target|.
          image_d3d->BindTexImage(target);
        }

        texture->clear_bind_pending();
      }
    }
  }
  bool write_access = mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  return d3d_image_backing->BeginAccessD3D11(write_access);
}

void GLTexturePassthroughD3DImageRepresentation::EndAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

#if BUILDFLAG(USE_DAWN)
DawnD3DImageRepresentation::DawnD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      dawn_procs_(dawn::native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

DawnD3DImageRepresentation::~DawnD3DImageRepresentation() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture DawnD3DImageRepresentation::BeginAccess(WGPUTextureUsage usage) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  texture_ = d3d_image_backing->BeginAccessDawn(device_, usage);
  return texture_;
}

void DawnD3DImageRepresentation::EndAccess() {
  if (!texture_)
    return;

  // Do this before further operations since those could end up destroying the
  // Dawn device and we want the fence to be duplicated before then.
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessDawn(device_, texture_);

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}
#endif  // BUILDFLAG(USE_DAWN)

OverlayD3DImageRepresentation::OverlayD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : OverlayImageRepresentation(manager, backing, tracker) {}

OverlayD3DImageRepresentation::~OverlayD3DImageRepresentation() = default;

bool OverlayD3DImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  return static_cast<D3DImageBacking*>(backing())->BeginAccessD3D11(
      /*write_access=*/false);
}

void OverlayD3DImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  static_cast<D3DImageBacking*>(backing())->EndAccessD3D11();
}

absl::optional<gl::DCLayerOverlayImage>
OverlayD3DImageRepresentation::GetDCLayerOverlayImage() {
  return static_cast<D3DImageBacking*>(backing())->GetDCLayerOverlayImage();
}

D3D11VideoDecodeImageRepresentation::D3D11VideoDecodeImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture)
    : VideoDecodeImageRepresentation(manager, backing, tracker),
      texture_(std::move(texture)) {}

D3D11VideoDecodeImageRepresentation::~D3D11VideoDecodeImageRepresentation() =
    default;

bool D3D11VideoDecodeImageRepresentation::BeginWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  if (!d3d_image_backing->BeginAccessD3D11(/*write_access=*/true))
    return false;

  return true;
}

void D3D11VideoDecodeImageRepresentation::EndWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
D3D11VideoDecodeImageRepresentation::GetD3D11Texture() const {
  return texture_;
}

}  // namespace gpu
