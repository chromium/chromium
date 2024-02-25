// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_representation.h"

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

GLTexturePassthroughD3DImageRepresentation::
    GLTexturePassthroughD3DImageRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
        std::vector<scoped_refptr<D3DImageBacking::GLTextureHolder>>
            gl_texture_holders)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      d3d11_device_(std::move(d3d11_device)),
      gl_texture_holders_(std::move(gl_texture_holders)) {}

GLTexturePassthroughD3DImageRepresentation::
    ~GLTexturePassthroughD3DImageRepresentation() = default;

bool GLTexturePassthroughD3DImageRepresentation::
    NeedsSuspendAccessForDXGIKeyedMutex() const {
  return static_cast<D3DImageBacking*>(backing())->has_keyed_mutex();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughD3DImageRepresentation::GetTexturePassthrough(
    int plane_index) {
  return gl_texture_holders_[plane_index]->texture_passthrough();
}

void* GLTexturePassthroughD3DImageRepresentation::GetEGLImage() {
  DCHECK(format().is_single_plane());
  return gl_texture_holders_[0]->egl_image();
}

bool GLTexturePassthroughD3DImageRepresentation::BeginAccess(GLenum mode) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());

  const bool write_access =
      mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM;
  if (!d3d_image_backing->BeginAccessD3D11(d3d11_device_, write_access)) {
    return false;
  }

  for (auto& gl_texture_holder : gl_texture_holders_) {
    // Bind GLImage to texture if it is necessary.
    gl_texture_holder->BindEGLImageToTexture();
  }

  return true;
}

void GLTexturePassthroughD3DImageRepresentation::EndAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11(d3d11_device_);
}

#if BUILDFLAG(USE_DAWN)
DawnD3DImageRepresentation::DawnD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      backend_type_(backend_type) {
  DCHECK(device_);
}

DawnD3DImageRepresentation::~DawnD3DImageRepresentation() {
  EndAccess();
}

wgpu::Texture DawnD3DImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  texture_ = d3d_image_backing->BeginAccessDawn(device_, backend_type_, usage);
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
  texture_.Destroy();
  texture_ = nullptr;
}
#endif  // BUILDFLAG(USE_DAWN)

OverlayD3DImageRepresentation::OverlayD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device)
    : OverlayImageRepresentation(manager, backing, tracker),
      d3d11_device_(std::move(d3d11_device)) {}

OverlayD3DImageRepresentation::~OverlayD3DImageRepresentation() = default;

bool OverlayD3DImageRepresentation::BeginReadAccess(
    gfx::GpuFenceHandle& acquire_fence) {
  return static_cast<D3DImageBacking*>(backing())->BeginAccessD3D11(
      d3d11_device_, /*write_access=*/false);
}

void OverlayD3DImageRepresentation::EndReadAccess(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  static_cast<D3DImageBacking*>(backing())->EndAccessD3D11(d3d11_device_);
}

std::optional<gl::DCLayerOverlayImage>
OverlayD3DImageRepresentation::GetDCLayerOverlayImage() {
  return static_cast<D3DImageBacking*>(backing())->GetDCLayerOverlayImage();
}

D3D11VideoDecodeImageRepresentation::D3D11VideoDecodeImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture)
    : VideoDecodeImageRepresentation(manager, backing, tracker),
      d3d11_device_(std::move(d3d11_device)),
      d3d11_texture_(std::move(d3d11_texture)) {}

D3D11VideoDecodeImageRepresentation::~D3D11VideoDecodeImageRepresentation() =
    default;

bool D3D11VideoDecodeImageRepresentation::BeginWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  if (!d3d_image_backing->BeginAccessD3D11(d3d11_device_,
                                           /*write_access=*/true)) {
    return false;
  }
  return true;
}

void D3D11VideoDecodeImageRepresentation::EndWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11(d3d11_device_);
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
D3D11VideoDecodeImageRepresentation::GetD3D11Texture() const {
  return d3d11_texture_;
}

}  // namespace gpu
