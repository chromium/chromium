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

DawnD3DImageRepresentation::DawnD3DImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      backend_type_(backend_type),
      view_formats_(view_formats) {
  DCHECK(device_);
}

DawnD3DImageRepresentation::~DawnD3DImageRepresentation() {
  EndAccess();
}

wgpu::Texture DawnD3DImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  texture_ = d3d_image_backing->BeginAccessDawn(device_, backend_type_, usage,
                                                internal_usage, view_formats_);
  return texture_;
}

void DawnD3DImageRepresentation::EndAccess() {
  if (!texture_)
    return;

  // Do this before further operations since those could end up destroying the
  // Dawn device and we want the fence to be duplicated before then.
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessDawn(device_, texture_);

  texture_ = nullptr;
}

// Enabling this functionality reduces overhead in the compositor by lowering
// the frequency of begin/end access pairs. The semantic constraints for a
// representation being able to return true are the following:
// * It is valid to call BeginScopedReadAccess() concurrently on two
//   different representations of the same image
// * The backing supports true concurrent read access rather than emulating
//   concurrent reads by "pausing" a first read when a second read of a
//   different representation type begins, which requires that the second
//   representation's read finish within the scope of its GPU task in order
//   to ensure that nothing actually accesses the first representation
//   while it is paused. Some backings that support only exclusive access
//   from the SI perspective do the latter (e.g.,
//   ExternalVulkanImageBacking as its "support" of concurrent GL and
//   Vulkan access). SupportsMultipleConcurrentReadAccess() results in the
//   compositor's read access being long-lived (i.e., beyond the scope of
//   a single GPU task).
// The Graphite Skia representation returns true if the underlying Dawn
// representation does so. This representation meets both of the above
// constraints.
bool DawnD3DImageRepresentation::SupportsMultipleConcurrentReadAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  // KeyedMutex does not support concurrent read access.
  return !d3d_image_backing->has_keyed_mutex();
}

DawnD3DBufferRepresentation::DawnD3DBufferRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type)
    : DawnBufferRepresentation(manager, backing, tracker),
      device_(device),
      backend_type_(backend_type) {
  DCHECK(device_);
}

DawnD3DBufferRepresentation::~DawnD3DBufferRepresentation() {
  EndAccess();
}

wgpu::Buffer DawnD3DBufferRepresentation::BeginAccess(wgpu::BufferUsage usage) {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  buffer_ =
      d3d_image_backing->BeginAccessDawnBuffer(device_, backend_type_, usage);
  return buffer_;
}

void DawnD3DBufferRepresentation::EndAccess() {
  if (!buffer_) {
    return;
  }

  // Do this before further operations since those could end up destroying the
  // Dawn device and we want the fence to be duplicated before then.
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessDawnBuffer(device_, buffer_);

  // All further operations on the buffer are errors (they would be racy
  // with other backings).
  buffer_ = nullptr;
}

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

#if DCHECK_IS_ON()
  // Sanity check that we can get the availability fence, meaning that the
  // texture is either immediately available or soon-to-be available. We
  // should not cache this since the eventual wait may be one or more frames
  // later and the fence becomes invalidated by DComp commit.
  std::ignore = static_cast<D3DImageBacking*>(backing())
                    ->GetDCompTextureAvailabilityFenceForCurrentFrame();
#endif
}

std::optional<gl::DCLayerOverlayImage>
OverlayD3DImageRepresentation::GetDCLayerOverlayImage() {
  return static_cast<D3DImageBacking*>(backing())->GetDCLayerOverlayImage();
}

D3D11VideoImageRepresentation::D3D11VideoImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture)
    : VideoImageRepresentation(manager, backing, tracker),
      d3d11_device_(std::move(d3d11_device)),
      d3d11_texture_(std::move(d3d11_texture)) {}

D3D11VideoImageRepresentation::~D3D11VideoImageRepresentation() = default;

bool D3D11VideoImageRepresentation::BeginWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  if (!d3d_image_backing->BeginAccessD3D11(d3d11_device_,
                                           /*write_access=*/true)) {
    return false;
  }
  return true;
}

void D3D11VideoImageRepresentation::EndWriteAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11(d3d11_device_);
}

bool D3D11VideoImageRepresentation::BeginReadAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  if (!d3d_image_backing->BeginAccessD3D11(d3d11_device_,
                                           /*write_access=*/false)) {
    return false;
  }
  return true;
}

void D3D11VideoImageRepresentation::EndReadAccess() {
  D3DImageBacking* d3d_image_backing = static_cast<D3DImageBacking*>(backing());
  d3d_image_backing->EndAccessD3D11(d3d11_device_);
}

Microsoft::WRL::ComPtr<ID3D11Texture2D>
D3D11VideoImageRepresentation::GetD3D11Texture() const {
  return d3d11_texture_;
}

}  // namespace gpu
