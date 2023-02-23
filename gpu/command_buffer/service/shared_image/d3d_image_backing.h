// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/d3d_shared_fence.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_image_d3d.h"

// Usage of BUILDFLAG(USE_DAWN) needs to be after the include for
// ui/gl/buildflags.h
#if BUILDFLAG(USE_DAWN)
#include <dawn/native/D3D12Backend.h>
using dawn::native::d3d12::ExternalImageDescriptorDXGISharedHandle;
using dawn::native::d3d12::ExternalImageDXGI;
using dawn::native::d3d12::ExternalImageDXGIBeginAccessDescriptor;
using dawn::native::d3d12::ExternalImageDXGIFenceDescriptor;
#endif  // BUILDFLAG(USE_DAWN)

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
struct Mailbox;

// Implementation of SharedImageBacking that holds buffer (front buffer/back
// buffer of swap chain) texture (as gles2::Texture/gles2::TexturePassthrough)
// and a reference to created swap chain.
class GPU_GLES2_EXPORT D3DImageBacking
    : public ClearTrackingSharedImageBacking {
 public:
  // Create a backing wrapping given D3D11 texture, optionally with a shared
  // handle and keyed mutex state. Array slice is used to specify index in
  // texture array used by video decoder and plane index is used to specify the
  // plane (Y/0 or UV/1) in NV12/P010 video textures.
  static std::unique_ptr<D3DImageBacking> Create(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state,
      GLenum texture_target,
      size_t array_slice = 0u,
      size_t plane_index = 0u);

  static std::unique_ptr<D3DImageBacking> CreateFromSwapChainBuffer(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
      bool is_back_buffer);

  // TODO(sunnyps): Remove this after migrating DXVA decoder to EGLImage.
  static std::unique_ptr<D3DImageBacking> CreateFromGLTexture(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      scoped_refptr<gles2::TexturePassthrough> gl_texture,
      size_t array_slice);

  // Helper used by D3D11VideoDecoder to create backings directly.
  static std::vector<std::unique_ptr<SharedImageBacking>>
  CreateFromVideoTexture(
      base::span<const Mailbox> mailboxes,
      DXGI_FORMAT dxgi_format,
      const gfx::Size& size,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      unsigned array_slice,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state = nullptr);

  D3DImageBacking(const D3DImageBacking&) = delete;
  D3DImageBacking& operator=(const D3DImageBacking&) = delete;

  ~D3DImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool PresentSwapChain() override;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats) override;

  bool BeginAccessD3D11(bool write_access);
  void EndAccessD3D11();

#if BUILDFLAG(USE_DAWN)
  WGPUTexture BeginAccessDawn(WGPUDevice device, WGPUTextureUsage usage);
  void EndAccessDawn(WGPUDevice device, WGPUTexture texture);
#endif

  absl::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage();

  bool has_keyed_mutex() const {
    return dxgi_shared_handle_state_ &&
           dxgi_shared_handle_state_->has_keyed_mutex();
  }

  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_for_testing()
      const {
    return dxgi_shared_handle_state_;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_for_testing() const {
    return d3d11_texture_;
  }

  static scoped_refptr<gles2::TexturePassthrough> CreateGLTexture(
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      GLenum texture_target = GL_TEXTURE_2D,
      unsigned array_slice = 0u,
      unsigned plane_index = 0u,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr);

 protected:
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<VideoDecodeImageRepresentation> ProduceVideoDecode(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VideoDecodeDevice device) override;

 private:
#if BUILDFLAG(USE_DAWN)
  struct DawnExternalImageState {
    DawnExternalImageState();
    DawnExternalImageState(DawnExternalImageState&&);
    DawnExternalImageState& operator=(DawnExternalImageState&&);
    ~DawnExternalImageState();

    // If an external image exists, it means Dawn produced the D3D12 side of the
    // D3D11 texture created by ID3D12Device::OpenSharedHandle().
    std::unique_ptr<ExternalImageDXGI> external_image;

    // Signaled fence imported from Dawn at EndAccess. This can be reused if
    // D3DSharedFence::IsSameFenceAsHandle() is true for fence handle from Dawn.
    scoped_refptr<D3DSharedFence> signaled_fence;
  };
  base::flat_map<WGPUDevice, DawnExternalImageState> dawn_external_image_cache_;
#endif  // BUILDFLAG(USE_DAWN)

  D3DImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
      std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures,
      scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state = nullptr,
      GLenum texture_target = GL_TEXTURE_2D,
      size_t array_slice = 0u,
      size_t plane_index = 0u,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr,
      bool is_back_buffer = false);

  WGPUTextureUsageFlags GetAllowedDawnUsages(
      const WGPUTextureFormat wgpu_format) const;

  gl::GLImage* GetGLImage() const;

  ID3D11Texture2D* GetOrCreateStagingTexture();

  bool ValidateBeginAccess(bool write_access) const;

  void EndAccessCommon(scoped_refptr<D3DSharedFence> fence);

  // Texture could be nullptr if an empty backing is needed for testing.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture_;

  // Can be null for backings owned by non-GL producers e.g. WebGPU.
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures_;

  // Holds DXGI shared handle and the keyed mutex if present.  Can be shared
  // between plane shared image backings of a multi-plane texture, or between
  // backings created from duplicated handles that refer to the same texture.
  scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state_;

  // GL texture target. Can be GL_TEXTURE_2D or GL_TEXTURE_EXTERNAL_OES.
  // TODO(sunnyps): Switch to GL_TEXTURE_2D for all cases.
  GLenum texture_target_;

  // Index of texture slice in texture array e.g. those used by video decoder.
  const size_t array_slice_;

  // Texture plane index corresponding to this image.
  const size_t plane_index_;

  // Swap chain corresponding to this backing.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  // Set if this backing corresponds to the back buffer of |swap_chain_|.
  const bool is_back_buffer_;

  // Staging texture used for copy to/from shared memory GMB.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;

  // D3D11 device corresponding to the |d3d11_texture_| provided on creation.
  // TODO(sunnyps): Support multiple D3D11 devices.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  // Whether the backing is being used for exclusive read-write access.
  bool in_write_access_ = false;

  // Number of concurrent readers for this backing.
  int num_readers_ = 0;

  // Fences for previous reads. These will be waited on by the subsequent write,
  // but not by reads.
  base::flat_set<scoped_refptr<D3DSharedFence>> read_fences_;

  // Fence for the previous write. These will be waited on by subsequent reads
  // and/or write.
  scoped_refptr<D3DSharedFence> write_fence_;

  // Fence used for signaling on this backing's |d3d11_device_|. Lazily created
  // and signaled on first Dawn access, and used on any subsequent D3D11 access.
  // TODO(sunnyps): Support multiple D3D11 devices.
  scoped_refptr<D3DSharedFence> d3d11_device_fence_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_H_
